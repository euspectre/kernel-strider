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
 *   'tindex' - it will contain the index of the current thread if sampling
 *   is enabled, 0 otherwise;
 *   'fi' - it will contain the address of the func_info instance for the
 *   function (passed to the function as a parameter).
 *  
 * Parameter: 
 *   struct kedr_prologue_data *pd - address of a structure containing a 
 *   pointer to the func_info instance for the function as well as the data
 *   needed to obtain the arguments of the function, etc. 
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

/* kedr_on_common_block_end 
 * Called after a common block containing one or more tracked memory 
 * operations ends. Calls the user-defined handlers (if present):
 * begin_memory_events(), end_memory_events(), on_memory_event().
 *
 * On entry, local_storage::info should be the address of the block_info
 * instance for the block. The fields 'values[]', 'tid', 'write_mask' are 
 * also used when necessary.
 * [NB] If some address stored in 'values[]' is NULL, it is assumed that the
 * corresponding memory operation did not happen.
 *
 * After the function has called all the appropriate handlers for the block,
 * 'values[]', 'write_mask' and 'dest_addr' are zeroed in the local storage,
 * other fields remain unchanged. This prepares the local storage for the 
 * execution of a subsequent code block.
 * 
 * Parameter:
 *   unsigned long storage - address of the local storage.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_on_common_block_end);

/* kedr_on_locked_op_pre
 * Called before the locked update operation. The operation is expected to 
 * be the only one in the block.
 * Calls on_locked_op_pre() if that handler is present, with the address 
 * of local_storage::temp as 'pdata'. That handler may store some data there
 * that the corresponding post handler might need (see below).
 *
 * After this function has been called, local_storage::temp must not be used
 * in the instrumented code until the corresponding post handler is called.
 *
 * On entry, local_storage::info should be the address of the block_info
 * instance for the block. 
 * 
 * Parameter:
 *   unsigned long storage - address of the local storage.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_on_locked_op_pre);

/* kedr_on_locked_op_post
 * Called after the locked update operation. The operation is expected to 
 * be the only one in the block.
 * Calls on_locked_op_post() if that handler is present, with 
 * local_storage::temp as 'data'. The corresponding pre handler might have 
 * stored some data in that field that the post handler needs.
 *
 * On entry, local_storage::info should be the address of the block_info
 * instance for the block. The fields 'values[]', 'tid', 'write_mask' are 
 * also used when necessary.
 *
 * After the function has called the appropriate handler, 'values[0]' and
 * 'write_mask' are zeroed in the local storage, other fields remain 
 * unchanged. This prepares the local storage for the execution of a 
 * subsequent code block.
 * [NB] It is enough to clear only values[0] rather than the whole values[]
 * because the operation is alone in the block, has only one memory access
 * (even CMPXCHG*) and it cannot be a string operation, see the list of 
 * the operations that can be locked in the description of LOCK in the 
 * Intel's manual, vol. 2A. 
 * 
 * Parameter:
 *   unsigned long storage - address of the local storage.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_on_locked_op_post);

/* kedr_on_io_mem_op_pre
 * Called before the I/O operation accessing memory. The operation is
 * expected to be the only one in the block.
 * Calls on_io_mem_op_pre() if that handler is present, with the address 
 * of local_storage::temp as 'pdata'. That handler may store some data there
 * that the corresponding post handler might need (see below).
 *
 * After this function has been called, local_storage::temp must not be used
 * in the instrumented code until the corresponding post handler is called.
 *
 * On entry, local_storage::info should be the address of the block_info
 * instance for the block. 
 * 
 * Parameter:
 *   unsigned long storage - address of the local storage.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_on_io_mem_op_pre);

/* kedr_on_io_mem_op_post
 * Called after the I/O operation accessing memory. The operation is 
 * expected to be the only one in the block.
 * Calls on_io_mem_op_post() if that handler is present, with 
 * local_storage::temp as 'data'. The corresponding pre handler might have 
 * stored some data in that field that the post handler needs.
 *
 * On entry, local_storage::info should be the address of the block_info
 * instance for the block. The fields 'values[]' and 'tid', are also used 
 * when necessary. 
 *
 * After the function has called the appropriate handler, 'values[0]' and
 * values[1] are zeroed in the local storage, other fields remain unchanged.
 * This prepares the local storage for the execution of a subsequent code 
 * block.
 * [NB] As the operation in this block is either INS or OUTS, we only need
 * to clear the first two elements of 'values[]'. 'write_mask' must remain
 * 0 anyway (it is only changed by CMPXCHG* which must not occur here), so 
 * there is no need to clear it.
 * 
 * Parameter:
 *   unsigned long storage - address of the local storage.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_on_io_mem_op_post);

/* kedr_on_barrier_pre
 * Called before a memory barrier operation which is not a tracked memory 
 * access. The operation is expected to be the only one in the block.
 * Calls on_memory_barrier_pre() if that handler is present.
 *
 * On entry, local_storage::temp should be the type of the barrier, 
 * local_storage::temp1 should be the value of PC (program counter) for the
 * original instruction. kedr_on_barrier_pre() does not change these values.
 * 
 * Parameter:
 *   unsigned long storage - address of the local storage.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_on_barrier_pre);

/* kedr_on_barrier_post
 * Called after a memory barrier operation which is not a tracked memory 
 * access. The operation is expected to be the only one in the block.
 * Calls on_memory_barrier_post() if that handler is present.
 *
 * On entry, local_storage::temp should be the type of the barrier, 
 * local_storage::temp1 should be the value of PC (program counter) for the
 * original instruction.  
 * 
 * Parameter:
 *   unsigned long storage - address of the local storage.
 * Return value:
 *   none. */
KEDR_DECLARE_WRAPPER(kedr_on_barrier_post);
/* ====================================================================== */

#endif /* HANDLERS_H_1810_INCLUDED */
