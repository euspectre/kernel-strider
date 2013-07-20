/* local_storage.h - the structures and functions related to the local 
 * storage. */

#ifndef LOCAL_STORAGE_H_1122_INCLUDED
#define LOCAL_STORAGE_H_1122_INCLUDED

#include <linux/kernel.h>
#include <linux/gfp.h>

#include <kedr/kedr_mem/block_info.h>
#include <kedr/kedr_mem/functions.h>

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

/* Number of general-purpose registers. We define it here to avoid
 * dependency on the core's internal headers. */
#ifdef CONFIG_X86_64
# define KEDR_X86_REG_COUNT 16

#else /* CONFIG_X86_32 */
# define KEDR_X86_REG_COUNT 8

#endif

struct kedr_ls_regs
{
	unsigned long ax;
	unsigned long cx;
	unsigned long dx;
	unsigned long bx;
	unsigned long sp;
	unsigned long bp;
	unsigned long si;
	unsigned long di;

#ifdef CONFIG_X86_64
	unsigned long r8;
	unsigned long r9;
	unsigned long r10;
	unsigned long r11;
	unsigned long r12;
	unsigned long r13;
	unsigned long r14;
	unsigned long r15;
#endif
};

/* The registers that can be needed to obtain the arguments of an 
 * instrumented function. */
struct kedr_arg_regs
{
#ifdef CONFIG_X86_64
	unsigned long rdi;
	unsigned long rsi;
	unsigned long rdx;
	unsigned long rcx;
	unsigned long r8;
	unsigned long r9;
	unsigned long rsp;

#else /* x86-32 */
	unsigned long eax;
	unsigned long edx;
	unsigned long ecx;
	unsigned long esp;
#endif
};

/* The local storage. */
struct kedr_local_storage 
{
	/* Spill slots for the general-purpose registers. This is the place 
	 * where the values from a register can be temporarily stored while
	 * the register is used for some other purpose. 
	 * See arch/x86/include/kedr/asm/inat.h for the list of the register
	 * codes, these are to be used as the indexes into regs[] array.
	 * 
	 * This array is located at the beginning of the local storage 
	 * because this allows to address the spill slots using only 8-bit
	 * offsets from the beginning of the storage even on x86-64 systems
	 * (the offsets are signed 8-bit values).
	 * The largest offset is 
	 *     sizeof(unsigned long) * (KEDR_X86_REG_COUNT - 1)
	 * On x86-64, this is 8 * 15 = 120 < 127, which is the maximum 8-bit
	 * positive offset. */
	union {
		unsigned long regs[KEDR_X86_REG_COUNT];
		struct kedr_ls_regs r;
	};
	
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
	
	/* Information about the function. */
	struct kedr_func_info *fi;
	
	/* Needed for CMPXCHG* because it is not clear for these 
	 * instructions until runtime whether a write to memory will happen.
	 * For each such instruction, the corresponding bit of the mask 
	 * should be set to 1, the remaining bits should be 0.
	 * When handling the end of the corresponding block, use 
	 * kedr_local_storage::write_mask | kedr_block_info::write_mask to
	 * obtain the actual write mask. */
	unsigned long write_mask;
	
	/* When processing the end of the block with memory accesses to
	 * record, 'info' should be the address of the corresponding 
	 * instance of kedr_block_info.
	 * When processing a function call, 'info' is the address of the
	 * corresponding instance of kedr_call_info. 
	 * In all other cases, the value of 'info' cannot be used. */
	unsigned long info;
	
	/* Destination address of a jump, used to handle jumps out of the
	 * code block. */
	unsigned long dest_addr;
	
	/* Two more slots for temporary data. It can be handy if using 
	 * a register to store these data is not desirable. */
	unsigned long temp;
	unsigned long temp1;
	
	/* The following two values are needed when processing the function
	 * calls to hold the return value of the called function. The 
	 * function may use %rax or both %rax and %rdx to store that value,
	 * hence two slots are necessary here. */
	unsigned long ret_val;  	/* lower part, %rax */
	unsigned long ret_val_high;	/* higher part, %rdx */
	
	/* One more value needed when processing the function calls:
	 *  ret_addr - the saved intermediate return address for a 
	 * function call. */
	unsigned long ret_addr;
	
	/* The additional slots for the registers that can be used for 
	 * parameter transfer and therefore may be needed to obtain the 
	 * arguments of the instrumented function. The values in these slots
	 * are guaranteed to be preserved during the execution of the 
	 * function unlike the contents of the register spill slots defined
	 * above. The arguments of the function may be used in the exit 
	 * handler (to be exact, in a post handler called from there), so it
	 * is needed to preserve them. */
	struct kedr_arg_regs arg_regs;
	
	/* This slot can be used if it is necessary to pass data from a
	 * pre handler of some event to the corresponding post handler.
	 * One situation when this can be necessary is handling of a
	 * call to a function that receives its arguments on stack. The
	 * function is allowed to change the contents of these stack 
	 * areas at a whim. So if a post handler for that function needs
	 * some of these stack parameters, the pre handler could save 
	 * these in 'data' (or allocate a structure and save its address
	 * in data). 
	 *
	 * [NB] Do not use this field to pass data from the pre to the post
	 * handler of a callback function (see kedr_func_info::*_handler).
	 * The callback may call a function the handling of which involves 
	 * 'ls->data' and that would make a mess. 
	 *
	 * 'ls->data' is intended to be used in the handlers for the 
	 * exported functions. 
	 * Use 'ls->cbdata' in the handlers for callbacks to avoid 
	 * collisions. */
	unsigned long data;
	
	/* Similar to 'data' but for use in pre and post handlers of the 
	 * callbacks, see kedr_func_info::*_handler. */
	unsigned long cbdata;

	/* Index of the thread, an integer number assigned to each thread in
	 * the order the thread enters the target module.
	 * This field is used for sampling only. If sampling is disabled
	 * (see the description of 'sampling_rate' parameter of the core),
	 * this field will be 0. */
	unsigned long tindex;

	/* This field can be used by the function handling plugins to track
	 * the status of the locks without using 'data' field which may be
	 * needed for some other purpose.
	 * A plugin may, for example, use 'lock_status' as follows. For each
	 * callback in the target module known to execute under a particular
	 * lock, the pre handler might set a corresponding bit in
	 * 'lock_status' if it has emitted "lock" event and the post handler
	 * needs to emit "unlock" event. */
	unsigned long lock_status;
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
