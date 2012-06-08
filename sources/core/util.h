/* util.h - various utility stuff */

#ifndef UTIL_H_1633_INCLUDED
#define UTIL_H_1633_INCLUDED

#include <kedr/asm/insn.h> /* instruction analysis facilities */

#include "ifunc.h"
/* ====================================================================== */

struct module;
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

/* A special code with the meaning "no register". */
#define KEDR_REG_NONE   0xff
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
 * failure. kedr_for_each_insn() continues as long as there are instructions 
 * left and proc() returns 0. If proc() returns nonzero, for_each_insn()
 * stops and returns this value.
 * 
 * Use this function instead of explicit walking, decoding and processing 
 * the areas of code (you remember C++ and STL best practices, right?). */
int 
kedr_for_each_insn(unsigned long start_addr, unsigned long end_addr,
	int (*proc)(struct insn *, void *), void *data);

/* Similar to kedr_for_each_insn() but operates only on the given function 
 * 'func' (on its original code). 
 * 
 * Note that 'proc' callback must have a different prototype here:
 *   int <name>(struct kedr_ifunc *, struct insn *, void *)
 * That is, it will also get access to 'func' without the need for any
 * special wrapper structures (for_each_insn_in_function() handles wrapping
 * stuff itself). */
int
kedr_for_each_insn_in_function(struct kedr_ifunc *func, 
	int (*proc)(struct kedr_ifunc *, struct insn *, void *), 
	void *data);

/* Nonzero if 'addr' is an address of some location within the given 
 * function, 0 otherwise. */
static inline int
kedr_is_address_in_function(unsigned long addr, struct kedr_ifunc *func)
{
	return (addr >= func->info.addr && 
		addr < func->info.addr + func->size);
}

/* Returns the code of a register which is in 'mask_choose_from' (the 
 * corresponding bit is 1) but not in 'mask_used' (the corresponding bit is 
 * 0). The code is 0-7 on x86-32 and 0-15 on x86-64. If there are several
 * registers of this kind, it is unspecified which one of them is returned.
 * If there are no such registers, KEDR_REG_NONE is returned. 
 *
 * N.B. The higher bits of the masks must be cleared. */
u8 
kedr_choose_register(unsigned int mask_choose_from, unsigned int mask_used);

/* Similar to kedr_choose_register() but the chosen register is additionally
 * guaranteed to be different from %base. */
static inline u8 
kedr_choose_work_register(unsigned int mask_choose_from, 
	unsigned int mask_used, u8 base)
{
	return kedr_choose_register(mask_choose_from, 
		mask_used | X86_REG_MASK(base));
}
/* ====================================================================== */

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module in the "init" area, 0 otherwise. */
int
kedr_is_init_text_address(unsigned long addr, struct module *mod);

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module in the "core" area, 0 otherwise. */
int
kedr_is_core_text_address(unsigned long addr, struct module *mod);

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module (*.text* sections), 0 otherwise. */
static inline int
kedr_is_text_address(unsigned long addr, struct module *mod)
{
	return (kedr_is_core_text_address(addr, mod) || 
		kedr_is_init_text_address(addr, mod));
}

/* Nonzero if 'addr' is the address of some location in the "init" area of 
 * the module (may be code or data), 0 otherwise. */
int
kedr_is_init_address(unsigned long addr, struct module *mod);

/* Nonzero if 'addr' is the address of some location in the "core" area of 
 * the module (may be code or data), 0 otherwise. */
int
kedr_is_core_address(unsigned long addr, struct module *mod);
/* ====================================================================== */
#endif // UTIL_H_1633_INCLUDED
