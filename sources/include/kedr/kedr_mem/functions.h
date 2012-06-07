/* functions.h - definitions for the function call handling (call 
 * replacement, pre- and post-handlers, etc.). */

#ifndef FUNCTIONS_H_1133_INCLUDED
#define FUNCTIONS_H_1133_INCLUDED

#include <linux/list.h>
#include <linux/spinlock.h>

struct kedr_local_storage;

/* Information about how to process the given call to the target function.
 * For each near call and jump out of the function, a call_info instance 
 * should be allocated during the instrumentation. 
 * During the instrumentation, the fields 'list' and 'pc' are initialized 
 * in each call_info instance, the remaining fields - only for direct 
 * calls/jumps (for indirect calls/jumps, the remaining fields will be 
 * set in runtime). */
struct kedr_call_info
{
	/* call_info instances for a given instrumented function may be 
	 * linked into a list to simplify deletion. */
	struct list_head list;
	
	/* Address of the call in the original code. */
	unsigned long pc;
	
	/* Address of the function to be called from the original code. */
	unsigned long target;
	
	/* Address of the function to call instead of the target, with
	 * the same parameters. Set it the same as 'target' if replacement
	 * is not needed and the target should be called. Note that pre-
	 * and post-handlers will be called anyway. */
	unsigned long repl;

	/* A function to be called before the target/replacement. 
	 * If call events are to be reported, this function should call
	 * on_call_pre() from the current set of event handlers. 
	 * Must not be NULL. */
	void (*pre_handler)(struct kedr_local_storage *);

	/* A function to be called after the target/replacement. 
	 * If call events are to be reported, this function should call
	 * on_call_post() from the current set of event handlers. 
	 * Must not be NULL. */
	void (*post_handler)(struct kedr_local_storage *);
	
	/* For the pre- and post- handlers, 'tid' as well as the address 
	 * of this call_info structure itself will be available in the 
	 * local storage. */
};

/* Information about a function that can be needed in runtime. */
struct kedr_func_info
{
	/* Address of the original (i.e. not instrumented) function. */
	unsigned long addr;
	
	/* If set, these handlers will be called on entry to the function 
	 * and right before its exit, respectively. Unlike the handlers from
	 * call_info structures, these handlers are called no matter how the
	 * function itself is called (i.e. from within the target module or
	 * from some other part of the kernel). This can be handy when 
	 * dealing with callback functions. The handlers from call_info 
	 * structures, on the other hand, are called only when the function
	 * is called from the target module itself (i.e. from the 
	 * instrumented code).
	 *
	 * The handlers must not assume they are executed in a process 
	 * context. They must neither sleep nor cause reschedule in any 
	 * other way.
	 *
	 * [NB] These handlers can be set by the components other than the
	 * core of our system. It is the respinsibility of these components
	 * to agree on some policy on setting these handlers (whether to 
	 * set new handlers if some handlers have already been set, etc.) 
	 *
	 * Execution of these handlers will be performed in the RCU 
	 * read-side sections (consider the addresses of the handlers as the
	 * pointers to the RCU-protected resources, use rcu_dereference(), 
	 * etc.).
	 * 
	 * The code setting these handlers must follow the rules for the RCU
	 * write-side sections: take 'handler_lock' to serialize the updates
	 * and use rcu_assign_pointer(). */
	void (*pre_handler)(struct kedr_local_storage *);
	void (*post_handler)(struct kedr_local_storage *);
	
	/* A lock to protect the update of the handlers. */
	spinlock_t handler_lock;
};

struct module;

/* The main responsibility of the "Function Handlers" subsystem is to 
 * provide the handlers (replacements, pre- and post-handlers) for the 
 * functions to be processed. */
struct kedr_function_handlers
{
	/* The module that provides the handlers. */
	struct module *owner;
	
	/* Looks for the handlers for the target function with the 
	 * given start address ('call_info->target'). If found, sets the
	 * appropriate handler addresses in '*call_info' leaving the 
	 * remaining ones unchanged.
	 * If not found, leaves the whole '*call_info' unchanged. */
	void (*fill_call_info)(struct kedr_function_handlers *fh, 
		struct kedr_call_info *call_info);
	
	/* These functions are called after the target module has been 
	 * loaded (but before it begins its initialization) and, 
	 * respectively, after the target module has finished its cleanup 
	 * and is about to unload.
	 *
	 * "Function Handlers" subsystem may use these callbacks for 
	 * initialization and cleanup of its per-session data. */
	void (*on_target_loaded)(struct kedr_function_handlers *fh, 
		struct module *mod);
	void (*on_target_about_to_unload)(struct kedr_function_handlers *fh, 
		struct module *mod);
};

/* Replace the current implementation of "Function Handlers" subsystem with
 * the given one. If 'fh' is NULL, the default implementation (provided by
 * the core) will be restored. 
 * It is not allowed to change "Function Handlers" implementations if the
 * target module is loaded. 
 * The function returns 0 if successful, a negative error code otherwise. */
int
kedr_set_function_handlers(struct kedr_function_handlers *fh);

/* KEDR_LS_* macros can be used in pre and post handlers to get the 
 * appropriate arguments (and, in case of a post handler, the return value)
 * of the target function. 
 * 
 * KEDR_LS_ARGn(ls) returns the value of the argument #n (n starts from 1)
 * of the target function. The local storage (pointed to by 'ls') is used to
 * obtain that value. The KEDR_LS_ARGn(ls) returns the value as unsigned 
 * long.
 *
 * [NB] The saved value of %rsp/%esp is as it should be before the call 
 * to the target in the original code. That is, if the target receives some
 * of its arguments on stack, the saved %rsp/%esp will point to the first 
 * ("top-most") of such arguments.
 *
 * KEDR_LS_ARGn() macros can only be used if the function actually has the
 * corresponding argument. If, for example, KEDR_LS_ARG4() is used in a 
 * handler of some function and that function has 3 arguments, the behaviour
 * is undefined.
 *
 * The registers are saved in the local storage before the target function
 * is called. So it should be safe to use the corresponding KEDR_LS_ARGn()
 * macros both in the pre- and in the post-handler of that function. 
 *
 * As for the stack-based arguments, things are different. In a pre-handler
 * they have the same values as on entry to the target function but the 
 * latter may change them at will (they are local variables anyway). So be
 * careful when using the corresponding KEDR_LS_ARGn() in the post 
 * handlers. If it is necessary to access the initial value of such 
 * argument in a post handler, it can be useful to save the value of that
 * argument to 'ls->data' in a pre handler and use that saved value in the 
 * post handler.
 * 
 * Note that KEDR_LS_ARGn() cannot be used in the handlers for the functions
 * with variable argument lists. It seems that such functions get all their
 * arguments on stack. These macros also cannot be used for the functions 
 * with 'fastcall' attribute on x86-32.
 *
 * KEDR_LS_RET_VAL(ls) uses the local storage ('*ls') to obtain the return
 * value of the function, cast to unsigned long. To be exact, it returns 
 * the lower sizeof(unsigned long) bytes of that value. That is, if a 
 * function returns u64 on a 32-bit system, KEDR_LS_RET_VAL will return the 
 * lower 32 bits of its result. */
#ifdef CONFIG_X86_64
/* Assuming the common convention on x86-64, the first 6 parameters are 
 * passed in %rdi, %rsi, %rdx, %rcx, %r8 and %r9, in order. The remaining
 * ones are passed on stack. */
# define KEDR_LS_ARG1(_ls) ((_ls)->r.di)
# define KEDR_LS_ARG2(_ls) ((_ls)->r.si)
# define KEDR_LS_ARG3(_ls) ((_ls)->r.dx)
# define KEDR_LS_ARG4(_ls) ((_ls)->r.cx)
# define KEDR_LS_ARG5(_ls) ((_ls)->r.r8)
# define KEDR_LS_ARG6(_ls) ((_ls)->r.r9)
# define KEDR_LS_ARG7(_ls) \
	(*(unsigned long *)((_ls)->r.sp))
# define KEDR_LS_ARG8(_ls) \
	(*(unsigned long *)((_ls)->r.sp + sizeof(unsigned long)))

# define KEDR_LS_RET_VAL(_ls) ((_ls)->ret_val)

#else /* CONFIG_X86_32 */
/* Assuming -mregparm=3, i.e. the first 3 arguments are passed in %eax, 
 * %edx and %ecx, in order, the remaining ones are passed on stack.
 * See arch/x86/Makefile in the kernel sources. */
# define KEDR_LS_ARG1(_ls) ((_ls)->r.ax)
# define KEDR_LS_ARG2(_ls) ((_ls)->r.dx)
# define KEDR_LS_ARG3(_ls) ((_ls)->r.cx)

/* The 4th and the following arguments are passed on stack. We assume the 
 * return address to be at the top of the stack, the item immediately below 
 * it is the argument #4 and so forth. 
 * The saved value of %esp is used to get to the arguments. */
# define KEDR_LS_ARG4(_ls) \
	(*(unsigned long *)((_ls)->r.sp))
# define KEDR_LS_ARG5(_ls) \
	(*(unsigned long *)((_ls)->r.sp + sizeof(unsigned long)))
# define KEDR_LS_ARG6(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 2 * sizeof(unsigned long)))
# define KEDR_LS_ARG7(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 3 * sizeof(unsigned long)))
# define KEDR_LS_ARG8(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 4 * sizeof(unsigned long)))

# define KEDR_LS_RET_VAL(_ls) ((_ls)->ret_val)

#endif /* ifdef CONFIG_X86_64 */
/* ====================================================================== */

/* API for the plugins that extend the function handling subsystem. 
 * Currently, no more than one plugin can be used with the latter at a 
 * time.
 * The plugins provide replacements for some of the functions called by the 
 * target module. */

struct kedr_repl_pair
{
	/* The start address of the original function. */
	void *orig; 
	
	/* The start address of the function to replace the original one 
	 * with. The replacement function must have the same signature as 
	 * the original function. */
	void *repl; 
};
 
struct kedr_fh_plugin
{
	/* The module that provides the plugin. It will be locked in the
	 * memory for the time the target module is in memory. */
	struct module *owner;
	
	/* These functions are called after the target module has been 
	 * loaded (but before it begins its initialization) and, 
	 * respectively, after the target module has finished its cleanup 
	 * and is about to unload. */
	void (*on_target_loaded)(struct module *mod);
	void (*on_target_about_to_unload)(struct module *mod);
	
	/* If this callback is set (i.e. is not NULL), it will be called 
	 * right before the target module 'mod' calls its exit function.
	 * If the target does not have an exit function, the callback
	 * will never be called. 
	 * 
	 * This callback can be used, for example, to establish a happens-
	 * before relationship between some callback operations and the 
	 * beginning of the module's cleanup (e.g. file operations that
	 * must always complete before the cleanup can start). */
	void (*on_before_exit_call)(struct module *mod);
	
	/* The "replacement table". Each element in this array specifies
	 * which function should be replaced by which function. The last 
	 * element of the array should have its 'orig' field set to NULL. 
	 * If 'repl_pairs' itself is NULL, this is treated as if the array
	 * was empty (i.e. contained only the "end marker" element). 
	 *
	 * The replacement table is owned by the plugin and must remain in
	 * place until the plugin unregisters itself. */
	struct kedr_repl_pair *repl_pairs;
};

/* Registration and deregistration of a plugin. 
 * The registration function returns 0 if successful, a negative error code
 * otherwise.
 * The functions cannot be called from atomic context. */
int
kedr_fh_plugin_register(struct kedr_fh_plugin *fh_plugin);

void 
kedr_fh_plugin_unregister(struct kedr_fh_plugin *fh_plugin);

#endif /* FUNCTIONS_H_1133_INCLUDED */
