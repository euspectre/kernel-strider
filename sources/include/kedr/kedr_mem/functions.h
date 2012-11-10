/* functions.h - definitions for the function call handling (call
 * replacement, pre- and post-handlers, etc.). */

#ifndef FUNCTIONS_H_1133_INCLUDED
#define FUNCTIONS_H_1133_INCLUDED

#include <linux/list.h>
#include <linux/spinlock.h>

struct kedr_local_storage;
struct kedr_session;
struct module;

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
	 * Must not be NULL. */
	void (*pre_handler)(struct kedr_local_storage *);

	/* A function to be called after the target/replacement.
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

	/* The kernel module this function belongs to. */
	struct module *owner;
	
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
	 * read-side section. Treat the addresses of the handlers as the
	 * pointers to the RCU-protected resources, use rcu_dereference(),
	 * etc.
	 *
	 * The code setting these handlers must follow the rules for the RCU
	 * write-side sections: take 'handler_lock' to serialize the updates
	 * and use rcu_assign_pointer().
	 *
	 * [NB] RCU seems to suit this well because the semantics is
	 * similar: there is a resource (a handler) that can be accessed via
	 * a pointer to it. Most of the accesses to the resource are
	 * expected to read it (execute the code), updates must be rare.
	 * Reclaim phase is not needed though. */
	void (*pre_handler)(struct kedr_local_storage *);
	void (*post_handler)(struct kedr_local_storage *);

	/* A lock to protect the update of the handlers. */
	spinlock_t handler_lock;
};

/* Searches for the func_info structure for a function with the given start
 * address. Returns NULL if not found.
 *
 * It is allowed to call this function only if the target module has already
 * been instrumented and is now in the memory. It is expected to be called
 * from function call handlers or function entry/exit handlers. */
struct kedr_func_info *
kedr_find_func_info(unsigned long addr);
/* ====================================================================== */

struct module;

struct kedr_fh_handlers
{
	/* The start address of the original function. */
	void *orig;

	/* If not NULL, this function will be called before the target
	 * function (the original or the replaced one). */
	void (*pre)(struct kedr_local_storage *);

	/* If not NULL, this function will be called after the target
	 * function (the original or the replaced one). */
	void (*post)(struct kedr_local_storage *);

	/* If not NULL, this is the start address of the function to be
	 * called from the target module instead of the original function.
	 * The replacement function must have the same signature as the
	 * original. */
	void *repl;
};

/* Function handling plugins (FH-plugins, in short) provide the functions to
 * be executed before, after and/or instead of the functions called from the
 * target module. */
struct kedr_fh_plugin
{
	/* The module that provides the plugin. It will be locked in the
	 * memory for the time the target module is in memory. */
	struct module *owner;

	/* The plugin structures may be arranged as a list by the core. */
	struct list_head list;

	/* The pointers to the handlers for the functions called by the 
	 * target module.
	 * If NULL, the array is treated the same as if it was empty. The
	 * NULL element marks the end if the array.
	 * 
	 * Note that unlike it is in KEDR, it is not currently allowed to
	 * have two or more plugins loaded at the same time if there is a
	 * function all these plugins provide handlers for.
	 * In the future versions, this limitation is likely to be removed.
	 *
	 * [NB] Once the plugin has been registered, it must not change the
	 * array of handlers. After the plugin has been unregistered, it may
	 * change the array of handlers if needed. */
	struct kedr_fh_handlers **handlers;
	
	/* Callbacks to module init and exit events. If a callback is not
	 * NULL it will be called when appropriate with the pointer to this
	 * kedr_fh_plugin instance and a pointer to 'struct module' for the
	 * target as the arguments. If a callback is NULL, it will be
	 * ignored.
	 *
	 * The core of the system ensures that such callbacks (be they from
	 * the same or from different plugins) are never executed
	 * concurrently.
	 *
	 * The callbacks are allowed to sleep / schedule. */

	/* Called after the target module has been loaded into memory but
	 * before it starts its initalization. */
	void (*on_init_pre)(struct kedr_fh_plugin *fh, struct module *mod);

	/* Called right after the target module has completed its
	 * initialization. 
	 * [NB] This callback may not be called if the target module has no 
	 * init function or if the init function is too small to be 
	 * instrumented. */
	void (*on_init_post)(struct kedr_fh_plugin *fh, struct module *mod);

	/* Called right before the target module starts executing its exit
	 * function. 
	 * [NB] This callback may not be called if the target module has no 
	 * exit function or if the exit function is too small to be 
	 * instrumented. */
	void (*on_exit_pre)(struct kedr_fh_plugin *fh, struct module *mod);

	/* Called when the target module has executed its exit function and
	 * is about to be unloaded. */
	void (*on_exit_post)(struct kedr_fh_plugin *fh, struct module *mod);
};

/* Registration and deregistration of a plugin.
 * The registration function returns 0 if successful, a negative error code
 * otherwise.
 * The functions cannot be called from atomic context. */
int
kedr_fh_plugin_register(struct kedr_fh_plugin *fh);

void
kedr_fh_plugin_unregister(struct kedr_fh_plugin *fh);
/* ====================================================================== */

/* KEDR_LS_* macros can be used in pre and post handlers to get the
 * appropriate arguments (and, in case of a post handler, the return value)
 * of the target function.
 *
 * KEDR_LS_ARGn(ls) returns the value of the argument #n (n starts from 1)
 * of the target function. The local storage (pointed to by 'ls') is used to
 * obtain that value. The KEDR_LS_ARGn(ls) returns the value as unsigned
 * long.
 *
 * [IMPORTANT]
 * If the target function has a variable argument list (e.g. snprintf()),
 * use KEDR_LS_ARGn_VA(ls) instead of KEDR_LS_ARGn(ls), even for the
 * arguments that are not variable. This is because on x86-32, such
 * functions receive all their arguments on stack. On x86-64, the
 * convention is the same as for the functions without variable argument
 * lists.
 *
 * Note that the rules for the functions that have 'va_list' as one of the
 * arguments (e.g. vsnprintf()) are the same as for the functions without
 * variable argument lists. 'va_list' is a single argument like any other.
 * That is, KEDR_LS_ARGn(ls) can and should be used in the handlers for
 * vsnprintf() but KEDR_LS_ARGn_VA(ls) should be used in the handlers for
 * snprintf().
 *
 * [NB] The saved value of %rsp/%esp is as it should be before the call
 * to the target in the original code. That is, if the target receives some
 * of its arguments on stack, the saved %rsp/%esp will point to the first
 * ("top-most") of such arguments.
 *
 * KEDR_LS_ARG*() macros can only be used if the function actually has the
 * corresponding argument. If, for example, KEDR_LS_ARG4() is used in a
 * handler of some function and that function has 3 arguments, the behaviour
 * is undefined.
 *
 * The registers are saved in the local storage before the target function
 * is called. So it should be safe to use the corresponding KEDR_LS_ARG*()
 * macros both in the pre- and in the post-handler of that function.
 *
 * As for the stack-based arguments, things are different. In a pre-handler,
 * they have the same values as on entry to the target function but the
 * latter may change them at will (they are local variables anyway). So be
 * careful when using the corresponding KEDR_LS_ARG*() in the post
 * handlers. If it is necessary to access the initial value of such
 * argument in a post handler, it can be useful to save the value of that
 * argument to 'ls->data' in a pre handler and use that saved value in the
 * post handler.
 *
 * Neither KEDR_LS_ARGn() nor KEDR_LS_ARGn_VA() can be used in the handlers
 * for the functions with 'fastcall' attribute on x86-32. Same for the
 * functions with floating-point parameters or the parameters to be stored
 * in SSE registers and the like. Such functions should be rare (if they
 * exist at all) in the kernel modules.
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

/* The convention for the functions with variable argument lists is the
 * same. */
# define KEDR_LS_ARG1_VA(_ls) KEDR_LS_ARG1(_ls)
# define KEDR_LS_ARG2_VA(_ls) KEDR_LS_ARG2(_ls)
# define KEDR_LS_ARG3_VA(_ls) KEDR_LS_ARG3(_ls)
# define KEDR_LS_ARG4_VA(_ls) KEDR_LS_ARG4(_ls)
# define KEDR_LS_ARG5_VA(_ls) KEDR_LS_ARG5(_ls)
# define KEDR_LS_ARG6_VA(_ls) KEDR_LS_ARG6(_ls)
# define KEDR_LS_ARG7_VA(_ls) KEDR_LS_ARG7(_ls)
# define KEDR_LS_ARG8_VA(_ls) KEDR_LS_ARG8(_ls)

/* The return value. */
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

/* The functions with variable argument lists (but not with 'va_list'
 * argument) have all their parameters on stack. */
# define KEDR_LS_ARG1_VA(_ls) \
	(*(unsigned long *)((_ls)->r.sp))
# define KEDR_LS_ARG2_VA(_ls) \
	(*(unsigned long *)((_ls)->r.sp + sizeof(unsigned long)))
# define KEDR_LS_ARG3_VA(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 2 * sizeof(unsigned long)))
# define KEDR_LS_ARG4_VA(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 3 * sizeof(unsigned long)))
# define KEDR_LS_ARG5_VA(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 4 * sizeof(unsigned long)))
# define KEDR_LS_ARG6_VA(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 5 * sizeof(unsigned long)))
# define KEDR_LS_ARG7_VA(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 6 * sizeof(unsigned long)))
# define KEDR_LS_ARG8_VA(_ls) \
	(*(unsigned long *)((_ls)->r.sp + 7 * sizeof(unsigned long)))

/* The return value. */
# define KEDR_LS_RET_VAL(_ls) ((_ls)->ret_val)

#endif /* ifdef CONFIG_X86_64 */
/* ====================================================================== */

#endif /* FUNCTIONS_H_1133_INCLUDED */
