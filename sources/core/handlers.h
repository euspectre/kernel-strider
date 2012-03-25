/* handlers.h - operations provided by the framework to be used in the 
 * instrumented code: handling of function entry and exit, etc.
 * Some of these operations may be used during the instrumentation as well.
 * The wrapper functions for these operations are also defined 
 * here. */

#ifndef HANDLERS_H_1810_INCLUDED
#define HANDLERS_H_1810_INCLUDED

/* We need to declare the wrappers to be able to use they addresses although
 * their definitions are inside the holders.
 *
 * The wrappers are declared as having no parameters and returning nothing 
 * because parameter passing and handling of the return value are to be done
 * manually in the machine code. 
 *
 * Each of the wrappers should be passed a single parameter via %eax/%rax. 
 * If a function returns a value, it will be in %eax/%rax too after the 
 * wrapper is finished.
 *
 * KEDR_DECLARE_WRAPPER(__func) declares a wrapper for function '__func'. */

#define KEDR_DECLARE_WRAPPER(__func) void __func ## _wrapper(void)
/* ====================================================================== */

/* kedr_on_function_entry
 * This function is called at the beginning of the instrumented function.
 * It allocates and initializes the local storage (see local_storage.h,
 * struct kedr_local_storage). 0 is returned if the allocation fails or 
 * some other error occurs.
 * 
 * The function also reports "function_entry" event (calls the appropriate
 * handler).
 *
 * When the local storage is initialized, all its fields will be zeroed
 * except the following ones:
 *   'tid' - it will contain the id of the current thread;
 *   'orig_func' - it will contain the address of the original instance of 
 *   the function (passed to the function as a parameter).
 *  
 * Parameter: 
 *   unsigned long orig_func - address of the original instance 
 *   of the function. It will be saved in the local storage for future
 *   reference.
 * Return value: 
 *   the address (unsigned long) of the allocated and properly intialized 
 *   local storage if successful, 0 on failure. */
KEDR_DECLARE_WRAPPER(kedr_on_function_entry);

/* kedr_on_function_exit
 * This function is called before the instrumented function exits. If the
 * latter has several exit points, the calls to kedr_process_function_exit() 
 * should be placed before each of these.
 * The function deallocates the local storage.
 *
 * The function also reports "function_exit" event (calls the appropriate
 * handler). 
 * 
 * Parameter:
 *   unsigned long pstorage - address of the local storage.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_on_function_exit);

/* kedr_fill_call_info
 * This function is used in handling of function calls. It looks up the 
 * handlers (pre-, post- and replacement) for the target function specified
 * in the kedr_call_info instance and sets the corresponding fields of
 * that instance.
 * If the function finds that no special processing is required for the 
 * given call, is sets the default handlers for that call. That is, the 
 * handlers are always set by this function.
 * 
 * Parameter:
 *   unsigned long ci - the address of kedr_call_info instance to fill.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_fill_call_info);

/* kedr_fill_call_info() can be used during the instrumentation too, without
 * any wrappers, of course. */
void
kedr_fill_call_info(unsigned long ci);

// TODO: add block end handlers here if needed.
/* ====================================================================== */

#endif // HANDLERS_H_1810_INCLUDED
