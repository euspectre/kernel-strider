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
 * target module is loaded. */
void
kedr_set_function_handlers(struct kedr_function_handlers *fh);

#endif // FUNCTIONS_H_1133_INCLUDED
