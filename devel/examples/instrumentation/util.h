/* util.h - various utility stuff */

#ifndef UTIL_H_1633_INCLUDED
#define UTIL_H_1633_INCLUDED

#include <kedr/asm/insn.h> /* instruction analysis facilities */

#include "ifunc.h"
/* ====================================================================== */

/* Opcodes for 'jmp rel32' and 'call rel32'. */
#define KEDR_OP_JMP_REL32	0xe9
#define KEDR_OP_CALL_REL32	0xe8

/* Size of 'call near rel 32' instruction, in bytes. */
#define KEDR_SIZE_CALL_REL32	5

/* Size of 'jmp rel32' machine instruction on x86 (both 32- and 64-bit).
 * This number of bytes at the beginning of each function of the target
 * module will be overwritten during the instrumentation. */
#define KEDR_SIZE_JMP_REL32 	5

/* Alignment of the start addresses of the instrumented functions (in 
 * bytes). The start address of the detour buffer will usually be 
 * page-aligned but it may also be desirable to align the start address of
 * each function. 
 *
 * KEDR_FUNC_ALIGN must be a power of 2. */
#define KEDR_FUNC_ALIGN 0x10UL

/* Align the value '_val', that is, round it up to the multiple of 
 * KEDR_FUNC_ALIGN. */
#define KEDR_ALIGN_VALUE(_val) \
  (((unsigned long)(_val) + KEDR_FUNC_ALIGN - 1) & ~(KEDR_FUNC_ALIGN - 1))
/* ====================================================================== */

/* For each instruction in [start_addr; end_addr), the function decodes it
 * and calls proc() callback for it. 'data' is passed to proc() as the 
 * last argument, it can be a pointer to the custom data needed by the 
 * particular callback. 
 *
 * [NB] The address of the instruction can be obtained in proc() via
 * insn->kaddr field.
 *
 * proc() is expected to return 0 on success and a negative error code on 
 * failure. for_each_insn() continues as long as there are instructions 
 * left and proc() returns 0. If proc() returns nonzero, for_each_insn()
 * stops and returns this value.
 * 
 * Use this function instead of explicit walking, decoding and processing 
 * the areas of code (you remember C++ and STL best practices, right?). */
int 
for_each_insn(unsigned long start_addr, unsigned long end_addr,
	int (*proc)(struct insn *, void *), void *data);

/* for_each_insn_in_function() - similar to for_each_insn() but operates 
 * only on the given function 'func' (on its original code). 
 * 
 * Note that 'proc' callback must have a different prototype here:
 *   int <name>(struct kedr_ifunc *, struct insn *, void *)
 * That is, it will also get access to 'func' without the need for any
 * special wrapper structures (for_each_insn_in_function() handles wrapping
 * stuff itself). */
int
for_each_insn_in_function(struct kedr_ifunc *func, 
	int (*proc)(struct kedr_ifunc *, struct insn *, void *), 
	void *data);

/* Nonzero if 'addr' is an address of some location within the given 
 * function, 0 otherwise. */
static inline int
is_address_in_function(unsigned long addr, struct kedr_ifunc *func)
{
	return (addr >= (unsigned long)func->addr && 
		addr < (unsigned long)func->addr + func->size);
}

#endif // UTIL_H_1633_INCLUDED
