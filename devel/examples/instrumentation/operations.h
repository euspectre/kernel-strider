/* operations.h - operations provided by the framework to be used in the 
 * instrumented code: processing of function entry and exit, etc.
 * The wrapper functions for these operations are also defined here. */

#ifndef OPERATIONS_H_1810_INCLUDED
#define OPERATIONS_H_1810_INCLUDED

/* We need to declare the wrappers to be able to use they addresses although
 * their definitions are inside the holders.
 *
 * The wrappers are declared as having no parameters and returning nothing 
 * because parameter passing and handling of the return value are to be done
 * manually in the machine code. 
 *
 * Each of the wrappers should be passed a single parameter via %eax/%rax. 
 * kedr_process_function_entry() and kedr_lookup_replacement() return the
 * result in %eax/%rax too, the other functions return nothing. */
/* ====================================================================== */

/* This function is called at the beginning of the instrumented function.
 * It allocates and initializes the primary storage (see primary_storage.h,
 * struct kedr_primary_storage). 0 is returned if the allocation fails or 
 * some other error occurs.
 * 
 * The function may also perform additional operations like calling a user-
 * defined function, recording the function entry in the trace, obtaining 
 * call stack, etc.
 *
 * When the primary storage is initialized, all its fields will be zeroed
 * except the following ones:
 *   'tid' - it will contain a thread id;
 *   'orig_func' - it will contain the address of the original instance of 
 *   the function (passed to kedr_process_function_entry as a parameter).
 *  
 * Parameter: 
 *   unsigned long orig_func - address of the original instance 
 *   of the function. It will be saved in the primary storage for future
 *   reference and can be used in some other way too.
 * Return value: 
 *   the address (unsigned long) of the allocated and properly intialized 
 *   primary storage if successful, 0 on failure. 
 */
void 
kedr_process_function_entry_wrapper(void);

/* This function is called before the instrumented function exits. If the
 * latter has several exit points, the calls to kedr_process_function_exit() 
 * will be placed for each of these.
 * The function deallocates the primary storage and may also perform other 
 * tasks like calling a user-defined function, recording the function exit 
 * in the trace, etc.
 * 
 * Parameter:
 *   unsigned long ps - address of the primary storage.
 * Return value:
 *   none. 
 */
void 
kedr_process_function_exit_wrapper(void);

/* This function is called after a normal code block ends. The function 
 * extracts the collected data from the storage (the records that have 
 * nonzero 'pc' value) and passes them to the output system ("flush"). The
 * data records, masks and 'dest_addr' are then reinitialized (zeroed) in
 * the primary storage, other fields remain unchanged. This prepares the 
 * primary storage for the execution of the subsequent code block.
 * 
 * Parameter:
 *   unsigned long ps - address of the primary storage.
 * Return value:
 *   none. 
 */
void 
kedr_process_block_end_wrapper(void);

/* This function is used in handling of function calls in the form of 
 * indirect calls and jumps. The start address of the about-to-be-called
 * function is known only in runtime, so if it is necessary to replace the 
 * call to this function with a call to a user-defined function with the 
 * same signature, this is also done in runtime in this case. 
 * kedr_lookup_replacement() returns the start address of the function that 
 * should actually be called. If no replacement should take place, it 
 * returns the same address as passed to it as a parameter.
 * 
 * Parameter:
 *   unsigned long addr - the start address of the function that was 
 *   originally meant to be called.
 * Return value:
 *   the start address of the function to be called (same as 'addr' if no
 *   replacement should take place, otherwise - the address of the 
 *   replacement function). 
 */
void 
kedr_lookup_replacement_wrapper(void);
/* ====================================================================== */

#endif // OPERATIONS_H_1810_INCLUDED
