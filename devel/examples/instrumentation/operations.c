/* operations.c - operations provided by the framework to be used in the 
 * instrumented code: processing of function entry and exit, etc.
 * The wrapper and holder functions for these operations are also defined 
 * here. */

#include <linux/kernel.h>

#include "operations.h"
#include "primary_storage.h"

/* ====================================================================== */

/* KEDR_SAVE_SCRATCH_REGS_BUT_AX
 * Save scratch registers (except %eax/%rax) and flags on the stack.
 * 
 * KEDR_RESTORE_SCRATCH_REGS_BUT_AX
 * Restore scratch registers (except %eax/%rax) and flags from the stack.
 *
 * KEDR_PREPARE_ARG_FROM_AX
 * On x86-32, does nothing because the first argument should be passed in 
 * %eax (if 'regparm' compiler option is used) and it is already there now. 
 * On x86-64, copies the value from %rax to %rdi where the first argument 
 * of the function should reside on this architecture. 
 * 
 * These macros can be used when injecting a function call into the 
 * code or making a function call from the instrumented code. The function 
 * to be called is a C function, so, according to x86 ABI, it is responsible
 * for preserving the values of the non-scratch registers. %eax/%rax should 
 * be saved and restored separately by the caller of the wrapper code. This 
 * register is used to pass the argument to the function to be called and 
 * to contain its return value. */
#ifdef CONFIG_X86_64
# define KEDR_SAVE_SCRATCH_REGS_BUT_AX \
	"pushfq\n\t"		\
	"pushq %rcx\n\t"	\
	"pushq %rdx\n\t"	\
	"pushq %rsi\n\t"	\
	"pushq %rdi\n\t"	\
	"pushq %r8\n\t"		\
	"pushq %r9\n\t"		\
	"pushq %r10\n\t"	\
	"pushq %r11\n\t"

# define KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
	"popq %r11\n\t"	\
	"popq %r10\n\t"	\
	"popq %r9\n\t"	\
	"popq %r8\n\t"	\
	"popq %rdi\n\t"	\
	"popq %rsi\n\t"	\
	"popq %rdx\n\t"	\
	"popq %rcx\n\t"	\
	"popfq\n\t"

# define KEDR_PREPARE_ARG_FROM_AX \
	"movq %rax, %rdi\n\t"
	
#else /* CONFIG_X86_32 */
# define KEDR_SAVE_SCRATCH_REGS_BUT_AX \
	"pushf\n\t"		\
	"pushl %ecx\n\t"	\
	"pushl %edx\n\t"

# define KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
	"popl %edx\n\t"		\
	"popl %ecx\n\t"		\
	"popf\n\t"

# define KEDR_PREPARE_ARG_FROM_AX ""
#endif /* #ifdef CONFIG_X86_64 */

/* The "holder-wrapper" technique is inspired by the implementation of 
 * KProbes (kretprobe, actually) on x86. 
 *
 * The wrappers below are used to inject the following function calls:
 * - kedr_process_function_entry
 * - kedr_process_function_exit
 * - kedr_process_block_end
 * - kedr_lookup_replacement
 * Each of this function accepts one parameter (type: unsigned long or 
 * pointer). The parameter is expected to be in %eax/%rax by the time the 
 * appropriate wrapper is called. The return value of the function will also
 * be stored in this register. Other general-purpose registers as well as 
 * flags will be preserved by the wrappers.
 *
 * These wrappers allow to reduce code bloat. If it were not for them, one
 * would need to insert the code for saving and restoring registers directly
 * into the instrumented function.
 *
 * The holder functions are not intended to be called. Their main purpose is 
 * to contain the wrapper functions written in assembly. The wrappers must 
 * not have usual prologues and epilogues unlike the ordinary functions. 
 * This is because we need to control which registers the wrapper functions
 * use, as well as when and how they do it.
 * 
 * [NB] "__used" makes the compiler think the function is used even if it is
 * not clearly visible where and how. This prevents the compiler from 
 * removing this function. */

/* KEDR_DEFINE_WRAPPER(__func) defines the wrapper for function '__func',
 * along with its holder. */
#define KEDR_DEFINE_WRAPPER(__func) 		\
static __used void __func ## _holder(void) 	\
{						\
	asm volatile (				\
		".global " #__func "_wrapper\n"	\
		#__func "_wrapper: \n\t"	\
		KEDR_SAVE_SCRATCH_REGS_BUT_AX	\
		KEDR_PREPARE_ARG_FROM_AX	\
		"call " #__func " \n\t"		\
		KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
		"ret\n");			\
}
/* ====================================================================== */

// TODO: implement the operations

/* The operations that can be used in the instrumented code.
 * These functions are static because they should only be called only via 
 * the wrappers. The description of these functions is given in operations.h
 * in the comments for the respective wrappers. */
static __used unsigned long
kedr_process_function_entry(unsigned long orig_func)
{
	// TODO
	return /*<>*/ 0; 
}
KEDR_DEFINE_WRAPPER(kedr_process_function_entry);

static __used void
kedr_process_function_exit(unsigned long ps)
{
	/*struct kedr_primary_storage *pstorage = 
		(struct kedr_primary_storage *)ps;*/
	// TODO
	return; 
}
KEDR_DEFINE_WRAPPER(kedr_process_function_exit);
		
static __used void
kedr_process_block_end(unsigned long ps)
{
	/*struct kedr_primary_storage *pstorage = 
		(struct kedr_primary_storage *)ps;*/

	// TODO
	return; 
}
KEDR_DEFINE_WRAPPER(kedr_process_block_end);

static __used unsigned long
kedr_lookup_replacement(unsigned long addr)
{
	// TODO
	return /*<>*/ addr; /* by default, do not replace the call address */
}
KEDR_DEFINE_WRAPPER(kedr_lookup_replacement);
/* ====================================================================== */
