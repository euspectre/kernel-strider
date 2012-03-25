/* insn_gen.h - generation of machine instructions needed for the 
 * instrumentation. */

#ifndef INSN_GEN_H_1759_INCLUDED
#define INSN_GEN_H_1759_INCLUDED

#include <linux/kernel.h>
#include <kedr/asm/insn.h> /* instruction analysis facilities */
#include "ir.h"

/* Each kedr_mk_*() function generates the requested machine instruction. 
 * If 'in_place' is 0, the function creates the IR node for that instruction
 * and inserts this node after the specified position in the IR (that is, 
 * new_node->list == item->next). If 'in_place' is non-zero, the function 
 * modifies the node (list_entry(item, ...)) in place. Note that when doing
 * in-place transformations, 'item' must be a field in a real entry rather 
 * than the head of the list.
 * 
 * The kedr_mk_*() function decodes the newly generated instruction and sets
 * 'insn' field in the corresponding node accordingly (in addition to 
 * 'insn_buffer').
 *
 * Return value: 'new_node->list' if a node has been added, or 'item' if the 
 * original node has been modified in place or if an error has occurred. 
 * Note that even if errors occur, the function returns the valid pointer 
 * to 'struct list_head' instance.
 *
 * [NB] kedr_mk_*() functions change neither 'first' nor 'last' in 
 * the IR nodes. 
 *
 * The last parameter of each kedr_mk_*() function ('err') is the pointer
 * to the current error code value. It must be a valid pointer. To simplify 
 * the implementation of the components that use sequences of the calls to 
 * kedr_mk_*() functions, these functions do nothing if *err != 0. That is, 
 * if an error is detected in one of these functions, it will be propagated.
 * The caller function should now be able to place the error handling code 
 * after the sequence of kedr_mk_*() calls rather than check for errors 
 * after each call in that sequence. 
 *
 * [NB] ESP/RSP and R12 can be used as the base register. At least, we 
 * do not require the reverse here. */
/* ====================================================================== */

/* mov %reg_from, %reg_to */
struct list_head *
kedr_mk_mov_reg_to_reg(u8 reg_from, u8 reg_to, struct list_head *item, 
	int in_place, int *err);

/* Store (mov %reg, <offset>(%base)) or load (mov <offset>(%base), %reg)
 * depending on 'is_load'. */
struct list_head *
kedr_mk_load_store_reg_mem(u8 reg, u8 base, unsigned long offset, 
	int is_load, struct list_head *item, int in_place, int *err);

/* Load data from the given position in the local storage ('ls') to the
 * register ('mov <offset>(%base), %reg'). */
static inline struct list_head *
kedr_mk_load_reg_from_mem(u8 reg, u8 base, unsigned long offset, 
	struct list_head *item, int in_place, int *err)
{
	return kedr_mk_load_store_reg_mem(reg, base, offset, 1, item, 
		in_place, err);
}

/* Store data from the register to the given position in the local 
 * storage ('mov %reg, <offset>(%base)'). */
static inline struct list_head *
kedr_mk_store_reg_to_mem(u8 reg, u8 base, unsigned long offset, 
	struct list_head *item, int in_place, int *err)
{
	return kedr_mk_load_store_reg_mem(reg, base, offset, 0, item, 
		in_place, err);
}

/* mov %reg, <offset_reg>(%base) */
struct list_head *
kedr_mk_store_reg_to_spill_slot(u8 reg, u8 base, struct list_head *item, 
	int in_place, int *err);

/* mov <offset_reg>(%base), %reg */
struct list_head *
kedr_mk_load_reg_from_spill_slot(u8 reg, u8 base, struct list_head *item, 
	int in_place, int *err);

/* push %reg */
struct list_head *
kedr_mk_push_reg(u8 reg, struct list_head *item, int in_place, int *err);

/* pop %reg */
struct list_head *
kedr_mk_pop_reg(u8 reg, struct list_head *item, int in_place, int *err);

/* call rel32, where rel32 is calculated for the destination address 'addr'.
 * This function can be used to create the calls to the wrapper functions.*/
struct list_head *
kedr_mk_call_rel32(unsigned long addr, struct list_head *item, int in_place, 
	int *err);

/* jcc near - a near conditional jump to an instruction represented by 
 * 'dest' node.
 * 'cc' - condition code (the 4 lower bits of the last opcode byte - see 
 * Intel's manual, vol. 2B, section B.1.4.7)
 * [NB] Invert the lower bit of the code <=> invert the condition. This 
 * could be useful when handling setcc and cmovcc instructions. */
struct list_head *
kedr_mk_jcc(u8 cc, struct kedr_ir_node *dest, struct list_head *item, 
	int in_place, int *err);

/* x86-32: see b8 (Move imm32 to r32.)
 * x86-64: see c7 (Move imm32 sign extended to 64-bits to r/m64.) */
struct list_head *
kedr_mk_mov_value32_to_ax(u32 value32, struct list_head *item, int in_place, 
	int *err);

/* test %reg, %reg */
struct list_head *
kedr_mk_test_reg_reg(u8 reg, struct list_head *item, int in_place, 
	int *err);

/* jmp near to a location at the address 'addr' outside of the current 
 * function.
 * This instruction can be used in the "entry call" to provide a jump 
 * to a fallback function if something goes wrong. */
struct list_head *
kedr_mk_jmp_to_external(unsigned long addr, struct list_head *item, 
	int in_place, int *err);

#ifndef CONFIG_X86_64
/* The instructions used on x86-32 only (handling of "pushad"/"popad") */

/* mov %eax, <offset_reg_on_stack>(%esp) or 
 * xchg %eax, <offset_reg_on_stack>(%esp), depending on 'is_xchg'.
 *
 * Updates the value of %reg saved by pushad with the value in %eax.
 * <offset_reg_on_stack>: 
 * The register #N (N = 0..7) is at the offset of 
 * ((7-N)*sizeof(unsigned long)) from %esp.
 *
 * If is_xchg is nonzero, 'xchg' instruction is used instead of 'mov' and
 * %eax will contain that original saved value of %reg as a result while the
 * new value will be stored in that slot on stack.
 *
 * Example: 87 44 24 e4: xchg   %eax,0x1c(%esp). */
struct list_head *
kedr_mk_mov_eax_to_reg_on_stack(u8 reg, int is_xchg, struct list_head *item, 
	int in_place, int *err);

/* mov <offset_base>(%base), %eax */
struct list_head *
kedr_mk_load_eax_from_base_slot(u8 base, struct list_head *item, 
	int in_place, int *err);

/* mov %eax, <offset_base>(%base) */
struct list_head *
kedr_mk_store_eax_to_base_slot(u8 base, struct list_head *item, 
	int in_place, int *err);
#endif

/* 'lea <expr>, %reg'
 * <expr> is the addressing expression taken (constructed) from 
 * 'src_node->insn' as is. 
 * [NB] If <expr> uses IP-relative addressing, the resulting node will have 
 * 'iprel_addr' set to the same address as in the source node. */
struct list_head *
kedr_mk_lea_expr_reg(struct kedr_ir_node *src_node, u8 reg, 
	struct list_head *item, int in_place, int *err);

/* 'mov <expr>, %reg'
 * <expr> is the addressing expression taken (constructed) from 
 * 'src_node->insn' as is. 
 * [NB] If <expr> uses IP-relative addressing, the resulting node will have 
 * 'iprel_addr' set to the same address as in the source node. */
struct list_head *
kedr_mk_mov_expr_reg(struct kedr_ir_node *src_node, u8 reg, 
	struct list_head *item, int in_place, int *err);

/* ret near */
struct list_head *
kedr_mk_ret(struct list_head *item, int in_place, int *err);

#ifdef CONFIG_X86_64
/* mov imm64, %rax
 * This instruction is needed when loading an immediate value to the local 
 * storage while moving the lower 32 bits of imm64 with sign extension 
 * cannot be used. In this case, we need to do it in two steps:
 *    mov imm64, %rax
 *    mov %rax, <offset>(%base) */
struct list_head *
kedr_mk_mov_imm64_to_rax(u64 imm64, struct list_head *item, int in_place, 
	int *err);
#endif 

/* mov value32, <offset>(%base) 
 * see c7 (Move imm32 sign extended to 64-bits to r/m64).
 * 
 * Can be used when handling jumps out of the normal block, etc. 
 * Sign extension helps when dealing with code addresses. */
struct list_head *
kedr_mk_mov_value32_to_slot(u32 value32, u8 base, u32 offset, 
	struct list_head *item, int in_place, int *err);

// TODO: more functions to generate necessary instructions

#endif // INSN_GEN_H_1759_INCLUDED
