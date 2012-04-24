/* functions.h - definitions for the function call handling (call 
 * replacement, pre- and post-handlers, etc.). */

#ifndef FUNCTIONS_H_1133_INCLUDED
#define FUNCTIONS_H_1133_INCLUDED

#include <linux/list.h>

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

struct module;

/* The main responsibility of the "Function Handlers" subsystem is to 
 * provide the handlers (replacements, pre- and post-handlers) for the 
 * functions to be processed. */
struct kedr_function_handlers
{
	/* The module that provides the handlers. */
	struct module *owner;
	
	/* Looks for the handlers for the target function with the 
	 * given start address ('call_info->target'). If found, fills the
	 * handler addresses in '*call_info' and returns non-zero. 
	 * If not found, returns 0 and leaves '*call_info' unchanged. */
	int (*fill_call_info)(struct kedr_function_handlers *fh, 
		struct kedr_call_info *call_info);
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

#endif /* FUNCTIONS_H_1133_INCLUDED */
