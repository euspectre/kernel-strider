/* local_storage.h - the structures and functions related to the local 
 * storage. */

#ifndef LOCAL_STORAGE_H_1122_INCLUDED
#define LOCAL_STORAGE_H_1122_INCLUDED

#include <linux/kernel.h>
#include <linux/gfp.h>

#include <kedr/asm/insn.h>	/* X86_REG_COUNT */
#include <kedr/kedr_mem/block_info.h>

/* The local storage is to some extent similar to thread-local storage (TLS)
 * widely used in the user space applications. The difference is that our 
 * local storage is allocated each time a kernel thread or the interrupt 
 * handler begins executing a function of the target module, and freed when 
 * the execution of that function ends. This way, several instances of the 
 * local storage for a given function may exist at the same time if several
 * threads execute that function simultaneously. */
 
/* The number of values the local storage can hold. The slots for these 
 * values in the storage can be used to store the addresses of the accessed
 * memory areas and sometimes (for string instructions) the sizes of these
 * memory areas. 
 * [NB] This constant must not exceed 32 in the current implementation. */
#define KEDR_MAX_LOCAL_VALUES 32

/* The local storage. */
struct kedr_local_storage 
{
	/* Spill slots for the general-purpose registers. This is the place 
	 * where the values from a register can be temporarily stored while
	 * the register is used for some other purpose. 
	 * See arch/x86/include/kedr/asm/inat.h for the list of the register
	 * codes, these are to be used as the indexes into this array.
	 * 
	 * This array is located at the beginning of the local storage 
	 * because this allows to address the spill slots using only 8-bit
	 * offsets from the beginning of the storage even on x86-64 systems
	 * (the offsets are signed 8-bit values).
	 * The largest offset is 
	 *     sizeof(unsigned long) * (X86_REG_COUNT - 1)
	 * On x86-64, this is 8 * 15 = 120 < 127, which is the maximum 8-bit
	 * positive offset. */
	unsigned long regs[X86_REG_COUNT];
	
	/* The slots for another group of local values. The addresses of 
	 * the accessed memory areas are stored here. For a string 
	 * operation, if values[i] is the address of the accessed area,
	 * values[i + 1] is the size of that area. For other operations, 
	 * the size can be determined at the instrumentation time and is
	 * therefore stored in struct kedr_block_info rather than here.
	 * kedr_block_info::string_mask can be used to determine how to 
	 * interpret the local values (in the handler of a block end). 
	 * [NB] If the address of an accessed memory area stored here is 
	 * NULL this indicates that the corresponding instruction did not 
	 * execute. */
	unsigned long values[KEDR_MAX_LOCAL_VALUES];
	
	/* "Thread ID", a unique number identifying the thread this storage
	 * belongs to. The interrupt handlers also have thread IDs, 
	 * different from the IDs of the "ordinary" threads. */
	unsigned long tid;
	
	/* Needed for CMPXCHG* because it is not clear for these 
	 * instructions until runtim whether a write to memory will happen.
	 * For each such instruction, the corresponding bit of the mask 
	 * should be set to 1, the remaining bits should be 0.
	 * When handling the end of the corresponding block, use 
	 * kedr_local_storage::write_mask | kedr_block_info::write_mask to
	 * obtain the actual write mask. */
	u32 write_mask;
	
	/* The address of the kedr_block_info structure for the block being
	 * processed. Can be NULL if the block has no operations to be 
	 * reported. */
	struct kedr_block_info *block_info;
	
	/* Destination address of a jump, used to handle jumps out of the
	 * code block. */
	unsigned long dest_addr;
	
	/* A place for temporary data. It can be handy if using a register
	 * to store these data is not desirable. */
	unsigned long temp;
	
	/* The following two values are needed when processing the function
	 * calls to hold the return value of the called function. The 
	 * function may use %rax or both %rax and %rdx to store that value,
	 * hence two slots are necessary here. */
	unsigned long ret_val;  	/* lower part, %rax */
	unsigned long ret_val_high;	/* higher part, %rdx */
	
	/* Three more values needed when processing the function calls:
	 *  ret_addr - the saved return address for a function call;
	 *  call_func - the address of the function to be called;
	 *  call_pc - the address of the instruction in the original code
	 *    that would perform that call. */
	unsigned long ret_addr;
	unsigned long call_func;
	unsigned long call_pc;
};

/* The allocator of kedr_local_storage instances. 
 * The core provides a default allocator which should be enough in most 
 * cases. Still, it can be replaced with a custom allocator if needed
 * (e.g. for optimization or testing purposes). 
 * 
 * When implementing a custom allocator, please take the following into 
 * account. 
 * 
 * 1. The functions provided by the allocator (alloc_ls() and free_ls()) get
 * the pointer to the allocator as an argument. If it is needed to pass 
 * some other data to these functions, 'kedr_ls_allocator' can be embedded 
 * in a larger structure and container_of() can be used in these functions
 * to get the address of that structure. 
 * 
 * 2. alloc_ls() should allocate a block of memory large enough to contain
 * struct kedr_local_storage and return a pointer to where the local storage
 * resides in that memory block. free_ls() should release the memory block 
 * containing the local storage instance 'ls' that was previously allocated 
 * with alloc_ls() of the same allocator. It is up to the implementation how 
 * the memory is actually managed: simple alloc/free each time, or something
 * like a mempool, or something with a garbage collector, or whatever else.
 *
 * 3. alloc_ls() is allowed to fail. If it fails to allocate memory, it 
 * should return NULL.
 *
 * 4. It is OK to call free_ls() with ls == NULL (similar to kfree()). 
 * free_ls() should be a no-op in this case.
 * 
 * 5. Both alloc_ls() and free_ls() should be suitable for execution in 
 * atomic context (and in interrupt context in particular). 
 * 
 * 6. alloc_ls() should zero memory occupied by a local storage instance it
 * has created. If alloc_ls() has allocated more memory than it is needed
 * to accomodate that instance, it is implementation-defined how the 
 * remaining parts of the allocated memory are to be filled. For example, it
 * is OK for the allocator to store some additional data beside the newly
 * created local storage instance, etc. 
 * 
 * 7. If the allocator is implemented in a different module rather than in 
 * the core itself, that module should remain live while the target module
 * is live in memory. 
 * 
 * 8. Both alloc_ls() and free_ls() should be thread-safe. That is, these 
 * functions can be called at any moment by any number of threads. */
struct module;
struct kedr_ls_allocator
{
	/* The module that provides the allocator. */
	struct module *owner;
	
	/* Allocate an instance of struct kedr_local_storage, zero memory
	 * occupied by it and return a pointer to it. NULL is returned if
	 * the allocation fails. */
	struct kedr_local_storage *(*alloc_ls)(
		struct kedr_ls_allocator *al);
	
	/* Free the instance of struct kedr_local_storage, previously 
	 * allocated with alloc_ls() function provided this allocator. */
	void (*free_ls)(struct kedr_ls_allocator *al, 
		struct kedr_local_storage *ls);
};

/* Replace the current allocator for local storage instances with the given 
 * one. If 'al' is NULL, the default allocator (provided by the core) will
 * be restored. 
 * It is not allowed to change the allocator if the target module is loaded.
 */
void
kedr_set_ls_allocator(struct kedr_ls_allocator *al);

/* Return the current allocator for local storage instances. This could be
 * used for testing, etc. The returned pointer will remain valid until the 
 * next call to kedr_set_ls_allocator(). 
 * The caller may call the methods of the returned allocator but must not 
 * change them. */
struct kedr_ls_allocator *
kedr_get_ls_allocator(void);

#endif // LOCAL_STORAGE_H_1122_INCLUDED
