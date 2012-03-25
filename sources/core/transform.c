/* transform.c - the functions to transform the appropriate parts of 
 * the IR during the instrumentation. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>

#include <kedr/kedr_mem/block_info.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>

#include <kedr/asm/insn.h>

#include "config.h"
#include "core_impl.h"

#include "ifunc.h"
#include "ir.h"
#include "transform.h"
#include "insn_gen.h"
#include "handlers.h"
#include "util.h"
#include "thunks.h"
/* ====================================================================== */

/* At the phase 2, if a handler adds nodes before or after the reference 
 * node 'ref_node', it should actually place the nodes before 
 * 'ref_node->first' or after 'ref_node->last', respectively. The handler is
 * responsible for updating these fields after that. */
/* ====================================================================== */

static void
warn_no_wreg(struct kedr_ir_node *node, u8 base)
{
	pr_warning(KEDR_MSG_PREFIX 
		"No registers left that can be chosen as a work register "
		"to handle the instruction at %pS (%%base: %u).\n",
		(void *)node->orig_addr, (unsigned int)base);
}

static void
warn_fail(struct kedr_ir_node *node)
{
	pr_warning(KEDR_MSG_PREFIX 
		"Failed to instrument the instruction at %pS.\n",
		(void *)node->orig_addr);
}

/* A helper function that generates the nodes for the following instructions
 * and places them after 'item': 
 * 
 * 	[mov   %treg, <offset_treg>(%base)] 
 *	[mov   %base, %treg]
 *	[mov   <offset_base>(%treg), %base]
 * 	mov   (<expr>), %<wreg>
 *	[mov   %treg, %base]
 *	[mov   <offset_treg>(%base), %treg]
 *
 * The instructions in square brackets are used only if <expr> uses the base 
 * register.
 * 
 * <expr> is a ModRM expression taken from the insn in 'ref_node';
 * %treg is a register not used in <expr> and different from %base and 
 * %wreg.
 * 
 * The function returns the last item created if successful. In case of an 
 * error, it also returns some valid item (the one passed as an argument 
 * or some item the function has created). This is similar to how kedr_mk_*
 * functions behave. As for 'err', 'mk_eval_addr_to_reg' handles it in the
 * same way as these functions do. */
static struct list_head *
mk_eval_addr_to_reg(struct kedr_ir_node *ref_node, u8 base, u8 wreg,
	struct list_head *item, int *err)
{
	u8 treg;
	unsigned int expr_reg_mask;
	int base_is_used;
	
	if (*err != 0)
		return item;
	
	expr_reg_mask = insn_reg_mask_for_expr(&ref_node->insn);
	base_is_used = (expr_reg_mask & X86_REG_MASK(base));
	
	treg = kedr_choose_work_register(X86_REG_MASK_ALL,
		(expr_reg_mask | 
			X86_REG_MASK(wreg) | 
			X86_REG_MASK(INAT_REG_CODE_SP)), 
		base);
	if (treg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		*err = -EILSEQ;
		return item;
	}
	
	if (base_is_used) {
		item = kedr_mk_store_reg_to_spill_slot(treg, base, item, 0, 
			err);
		item = kedr_mk_mov_reg_to_reg(base, treg, item, 0, err);
		item = kedr_mk_load_reg_from_spill_slot(base, treg, item, 0,
			err);
	}
	
	item = kedr_mk_mov_expr_reg(ref_node, wreg, item, 0, err);
	if (base_is_used) {
		item = kedr_mk_mov_reg_to_reg(treg, base, item, 0, err);
		item = kedr_mk_load_reg_from_spill_slot(treg, base, item, 0,
			err);
	}
	return item;
}
/* ====================================================================== */

/* ====================================================================== */
/* Transformation of the IR, phase 1 */
/* ====================================================================== */

/* The goal of the entry handler is to place the code at the entry of the 
 * function, that allocates the local storage and sets the base register.
 *   'ir' - the IR of the function.
 * 
 * Code:
 *	push  %rax
 *	mov   <orig_func_addr32>, %rax	# sign-extended on x86-64
 *	call  <kedr_on_function_entry_wrapper>
 *	test  %rax, %rax
 *	jnz   <go_on>
 *	pop   %rax
 *	jmp   <fallback_func>
 * go_on:
 * 	mov   %base, <offset_base>(%rax)
 *	mov   %rax, %base
 *	pop   %rax
 */
int
kedr_handle_function_entry(struct list_head *ir, struct kedr_ifunc *func, 
	u8 base)
{
	int err = 0;
	struct list_head *item = ir;	
	struct kedr_ir_node *jnz_node;
	struct list_head *go_on_item;
	
	jnz_node = kedr_ir_node_create();
	if (jnz_node == NULL)
		return -ENOMEM;
	
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_mov_value32_to_ax((u32)(unsigned long)func->addr, 
		item, 0, &err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_on_function_entry_wrapper, 
		item, 0, &err);
	item = kedr_mk_test_reg_reg(INAT_REG_CODE_AX, item, 0, &err);
	/* For now, add an empty node for 'jnz', we'll fill it later. */
	list_add(&jnz_node->list, item); 
	item = &jnz_node->list;
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_jmp_to_external((unsigned long)func->fallback, item,
		0, &err);
	item = kedr_mk_store_reg_to_spill_slot(base, INAT_REG_CODE_AX, item,
		0, &err);
	go_on_item = item;
	item = kedr_mk_mov_reg_to_reg(INAT_REG_CODE_AX, base, item, 0, &err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);

	if (err == 0) {
		/* Fill the node for 'jnz' properly. All the previous 
		 * kedr_mk_* operations have completed successfully, so
		 * 'go_on_item' must be the 'list' field of a real node. */
		struct kedr_ir_node *go_on_node = 
			list_entry(go_on_item, struct kedr_ir_node, list);
		kedr_mk_jcc(INAT_CC_NZ, go_on_node, &jnz_node->list, 
			1 /* do it in place */, &err);
	} 
	if (err != 0) {
		/* [NB] kedr_mk_jcc() may return an error too. */
		pr_warning (KEDR_MSG_PREFIX 
		"Failed to instrument the entry of the function %s().\n", 
			func->name);
	}
	return err;
}

/* Handling of a simple exit from the function (see ir.c, 
 * is_simple_function_exit().
 * 
 * Code to insert before the instruction sequence:
 * 	push  %rax
 * 	mov   %base, %rax
 * 	mov   <offset_base>(%rax), %base
 * 	call  <kedr_on_function_exit_wrapper>
 * 	pop   %rax
 */
int
kedr_handle_function_exit(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item = ref_node->list.prev;
	struct list_head *first_item;
	
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
	first_item = item;
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(base, INAT_REG_CODE_AX, 
		item, 0, &err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_on_function_exit_wrapper,
		item, 0, &err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	
	if (err == 0)
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}

/* Common part of processing of indirect calls and jumps outside
 * The instructions in square brackets are used only if the original 
 * instruction uses the base register.
 * 
 * The rules for 'err' and 'item' are the same as for kedr_mk_*() functions.
 * The function returns the address of the item for the first instruction
 * in '*first_item'.
 *
 * [NB] %wreg and %treg are the registers not used in <expr>.
 * %base, %wreg and %treg are different registers, as usual.
 * %wreg is different from %rax.
 *
 * Code:
 *      mov   %wreg, <offset_wreg>(%base) 
 *      [mov   %treg, <offset_treg>(%base)] 
 *      [mov   %base, %treg]
 *      [mov   <offset_base>(%treg), %base]
 *      mov   (<expr>), %<wreg>
 *      [mov   %treg, %base]
 *      [mov   <offset_treg>(%base), %treg]
 *
 *      # We need to save %rax to its spill slot anyway (the thunk needs 
 *      # that), so let's do it now. This also allows to use %rax as an 
 *      # additional work register here.
 *      mov   %rax, <offset_ax>(%base)
 *
 *      (x86-32 only) mov <call_info32>, %eax
 *      (x86-64 only) mov <call_info64>, %rax
 *      mov %rax, <offset_info>(%base)
 *
 *      mov %wreg, <offset_target>(%rax)
 *      mov <offset_wreg>(%base), %wreg
 *
 *      # %rax now contains the address of the call_info instance, that is
 *      # exactly what kedr_fill_call_info_wrapper expects.
 *      call kedr_fill_call_info_wrapper
 *      # All the fields of the call_info instance must have been filled at
 *      # this point.
 * 
 *      mov %base, %rax
 */
static struct list_head *
mk_common_jmp_call_indirect(struct kedr_ir_node *ref_node, u8 base,
	struct list_head *item, int *err, struct list_head **first_item)
{
	u8 wreg;
	unsigned int expr_reg_mask;
	
	if (*err != 0)
		return item;
	
	expr_reg_mask = insn_reg_mask_for_expr(&ref_node->insn);	
	wreg = kedr_choose_work_register(X86_REG_MASK_ALL, 
		(expr_reg_mask | X86_REG_MASK(INAT_REG_CODE_SP) | 
			X86_REG_MASK(INAT_REG_CODE_AX)), 
		base);
	if (wreg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		*err = -EILSEQ;
		return item;
	}
	item = kedr_mk_store_reg_to_spill_slot(wreg, base, item, 0, err);
	*first_item = item;
	
	item = mk_eval_addr_to_reg(ref_node, base, wreg, item, err);
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_AX, base, item,
		0, err);
	
#ifdef CONFIG_X86_64
	item = kedr_mk_mov_imm64_to_rax((unsigned long)ref_node->call_info,
		item, 0, err);
	
#else /* x86-32 */
	item = kedr_mk_mov_value32_to_ax((unsigned long)ref_node->call_info,
		item, 0, err);
#endif
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_AX, base, 
		offsetof(struct kedr_local_storage, info), item, 0, err);
	item = kedr_mk_store_reg_to_mem(wreg, INAT_REG_CODE_AX, 
		offsetof(struct kedr_call_info, target), item, 0, err);
	item = kedr_mk_load_reg_from_spill_slot(wreg, base, item, 0, err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_fill_call_info_wrapper, item, 0, err);
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, err);
	return item;
}

/* Processing call near indirect (call *<expr>)
 *
 * Code:
 *	<see the common part>
 *      call kedr_thunk_call # this will replace the original insn
 */
int
kedr_handle_call_indirect(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item = ref_node->list.prev;
	struct list_head *first_item = NULL;
	
	item = mk_common_jmp_call_indirect(ref_node, base, item, &err, 
		&first_item);
	kedr_mk_call_rel32((unsigned long)&kedr_thunk_call, &ref_node->list,
		1, &err);
	
	if (err == 0)
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}

/* Processing of the indirect near jumps (jmp *<expr>) that we know for 
 * certain to transfer control only within the instrumented instance of 
 * the function.
 * 
 * If the jump does not use %base, it is OK to leave it unchanged.
 * Otherwise the code below should be applied.
 * 
 * %wreg - a non-scratch register not used by the jump and not %rsp.
 * %treg - a register not used in <expr>.
 * %base, %wreg and %treg are different registers, as usual.
 *
 * Code:
 *      mov   %wreg, <offset_wreg>(%base) 
 * 	mov   %treg, <offset_treg>(%base)
 *	mov   %base, %treg
 *	mov   <offset_base>(%treg), %base
 * 	mov   (<expr>), %wreg
 *	mov   %treg, %base
 *	mov   <offset_treg>(%base), %treg
 *	push  %wreg
 *	mov   <offset_wreg>(%base),%wreg
 *	ret	# this will replace the original insn 
 */
int
kedr_handle_jmp_indirect_inner(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item;
	struct list_head *first_item;
	u8 wreg;
	u8 treg;
	unsigned int expr_reg_mask;
	
	/* If the jump does not use %base, leave it as it is. */
	if (!(ref_node->reg_mask & X86_REG_MASK(base)))
		return 0;
	
	expr_reg_mask = insn_reg_mask_for_expr(&ref_node->insn);
	wreg = kedr_choose_work_register(X86_REG_MASK_NON_SCRATCH, 
		(ref_node->reg_mask | X86_REG_MASK(INAT_REG_CODE_SP)), 
		base);
	if (wreg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		return -EILSEQ;
	}
	treg = kedr_choose_work_register(X86_REG_MASK_ALL,
		(expr_reg_mask | 
			X86_REG_MASK(wreg) | 
			X86_REG_MASK(INAT_REG_CODE_SP)), 
		base);
	if (treg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		return -EILSEQ;
	}
	
	item = ref_node->list.prev;
	item = kedr_mk_store_reg_to_spill_slot(wreg, base, item, 0, &err);
	first_item = item;
	item = kedr_mk_store_reg_to_spill_slot(treg, base, item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(base, treg, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(base, treg, item, 0, &err);
	item = kedr_mk_mov_expr_reg(ref_node, wreg, item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(treg, base, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(treg, base, item, 0, &err);
	item = kedr_mk_push_reg(wreg, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(wreg, base, item, 0, &err);
	/* replace the original insn with 'ret' */
	kedr_mk_ret(&ref_node->list, 1, &err);
	
	if (err == 0)
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}

/* Processing jmp near indirect (jmp *<expr>) that transfers control 
 * outside of the function.
 *
 * Code:
 *	<see the common part>
 *
 *      # Restore %base because this jump is an exit from the function.
 *      mov <offset_base>(%rax), %base
 *      jmp kedr_thunk_jmp # this will replace the original insn
 */
int
kedr_handle_jmp_indirect_out(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item = ref_node->list.prev;
	struct list_head *first_item = NULL;
	
	item = mk_common_jmp_call_indirect(ref_node, base, item, &err, 
		&first_item);
	item = kedr_mk_load_reg_from_spill_slot(base, INAT_REG_CODE_AX,
		item, 0, &err);
	kedr_mk_jmp_to_external((unsigned long)&kedr_thunk_jmp, 
		&ref_node->list, 1, &err);
	
	if (err == 0)
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}

/* Handling of a near relative call.
 * 
 * Code: 	
 *      mov %rax, <offset_ax>(%base)
 *      (x86-32 only) mov <call_info>, <offset_info>(%base)
 *      (x86-64 only) mov <call_info>, %rax
 *      (x86-64 only) mov %rax, <offset_info>(%base)
 *      mov %base, %rax
 *      call kedr_thunk_call # this will replace the original instruction
 */
int
kedr_handle_call_rel32_out(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item;
	struct list_head *first_item;
	
	BUG_ON(ref_node->call_info == NULL);
	
	item = ref_node->list.prev;
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_AX, base, item,
		0, &err);
	first_item = item;

#ifdef CONFIG_X86_64
	item = kedr_mk_mov_imm64_to_rax((unsigned long)ref_node->call_info,
		item, 0, &err);
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_AX, base, 
		offsetof(struct kedr_local_storage, info), item, 0, &err);
#else /* x86-32 */
	item = kedr_mk_mov_value32_to_slot(
		(unsigned long)ref_node->call_info, base,
		offsetof(struct kedr_local_storage, info), item, 0, &err);
#endif
	
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, 
		&err);
	/* Replace the original instruction with the call to the thunk. */
	kedr_mk_call_rel32((unsigned long)&kedr_thunk_call, &ref_node->list,
		1, &err);
	
	if (err == 0)
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}

/* Handling of a near relative jump, conditional or unconditional, that 
 * leads outside of the function. 
 * 
 * Code:  
 *      mov %rax, <offset_ax>(%base)
 *      (x86-32 only) mov <call_info>, <offset_info>(%base)
 *      (x86-64 only) mov <call_info>, %rax
 *      (x86-64 only) mov %rax, <offset_info>(%base)
 *      mov %base, %rax
 * 
 *      # Restore %base because this jump is an exit from the function.
 *      mov <offset_base>(%rax), %base
 *
 *      # Here goes the original instruction but with a different 
 *      # 'iprel_addr': it will change from the address of the target
 *      # to the address of the thunk. The immediate value in the 
 *	# instruction itself does not matter here.
 *      jxx kedr_thunk_jmp 
 */
int
kedr_handle_jxx_rel32_out(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item;
	struct list_head *first_item;
	
	BUG_ON(ref_node->call_info == NULL);
	
	item = ref_node->list.prev;
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_AX, base, item,
		0, &err);
	first_item = item;

#ifdef CONFIG_X86_64
	item = kedr_mk_mov_imm64_to_rax((unsigned long)ref_node->call_info,
		item, 0, &err);
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_AX, base, 
		offsetof(struct kedr_local_storage, info), item, 0, &err);
#else /* x86-32 */
	item = kedr_mk_mov_value32_to_slot(
		(unsigned long)ref_node->call_info, base,
		offsetof(struct kedr_local_storage, info), item, 0, &err);
#endif
	
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, 
		&err);
	item = kedr_mk_load_reg_from_spill_slot(base, INAT_REG_CODE_AX, 
		item, 0, &err);
	
	/* Change the destination of the jump to the thunk. */
	ref_node->iprel_addr = (unsigned long)&kedr_thunk_jmp;

	if (err == 0)
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}

#ifdef CONFIG_X86_64
/* Neither PUSHAD nor POPAD instructions are available on x86-64, so if 
 * these handlers are called there, it is our bug. */
int
kedr_handle_pushad(struct kedr_ir_node *ref_node, u8 base)
{
	BUG();
	return 0;
}

int
kedr_handle_popad(struct kedr_ir_node *ref_node, u8 base)
{
	BUG();
	return 0;
}

#else /* x86-32 */
/* Processing 'pushad' instruction.
 * 
 * First we execute the instruction, then we update the saved value of 
 * %base so that it is the same as the unbiased value of %base in the 
 * storage.
 * %eax is used as a work register.
 *
 * [NB] Finding <offset_of_base_on_stack>. The registers are pushed in 
 * their numeric order, this simplifies things a bit.
 * The register #N (N = 0..7) is at the offset of 
 * (+(7-N)*sizeof(unsigned long)) from %esp.
 * 
 * Code: 
 * 	<original_insn>
 *	mov %eax, <offset_ax>(%base)
 *	mov <offset_base>(%base), %eax
 *	mov %eax, <offset_of_base_on_stack>(%esp)
 *	mov <offset_ax>(%base), %eax
*/
int
kedr_handle_pushad(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item = &ref_node->list;
	
	/* This handler operates in the instruction itself, so it must be
	 * called before any other handler for this instruction. */
	BUG_ON(ref_node->first != ref_node || ref_node->last != ref_node);
	
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_AX, base, item,
		0, &err);
	item = kedr_mk_load_eax_from_base_slot(base, item, 0, &err);
	item = kedr_mk_mov_eax_to_reg_on_stack(base, 0, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(INAT_REG_CODE_AX, base, 
		item, 0, &err);
	
	if (err == 0)
		ref_node->last = 
			list_entry(item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}

/* Processing 'popad' instruction. 
 * 
 * First we update the saved value of %base so that it would not be
 * changed by 'popad'. 
 * In case someone changed the saved value of %base, we update the value
 * in the spill slot for %base in the storage.
 * %eax is used as a work register.
 *
 * [NB] Finding <offset_of_base_on_stack>. The registers are pushed in
 * their numeric order, this simplifies things a bit.
 * The register #N (N = 0..7) is at the offset of 
 * (+(7-N)*sizeof(unsigned long)) from %esp.
 *
 * Code: 
 *	mov %base, %eax
 *	xchg %eax, <offset_of_base_on_stack>(%esp)
 *	mov %eax, <offset_base>(%base)
 * 	<original insn>
 * We don't need to save %eax here as 'popad' will load a new value into it
 * anyway.
 */
int
kedr_handle_popad(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item = ref_node->list.prev;
	struct list_head *first_item;
	
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, &err);
	first_item = item;
	item = kedr_mk_mov_eax_to_reg_on_stack(base, 1, item, 0, &err);
	item = kedr_mk_store_eax_to_base_slot(base, item, 0, &err);
	
	if (err == 0)
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}
#endif

/* If the instruction in the node (<insn>) does not use %base, the function 
 * does nothing. Otherwise, the original value of %base is restored before 
 * the instruction. The code added by this handler ensures that at the end 
 * %base will contain the same value as on entry (the address of the local 
 * storage). In addition, the stored original value of %base is always
 * consistent.
 * 
 * It is expected that there is at least one register that <insn> does not 
 * use. %wreg should be chosen among such registers.
 * 
 * Code:
 *	mov %wreg, <offset_wreg>(%base)
 *	mov %base, %wreg
 *	mov <offset_base>(%wreg), %base
 *	<insn>
 *	mov %base, <offset_base>(%wreg)
 *	mov %wreg, %base
 *	mov <offset_wreg>(%base), %wreg
 */
int
kedr_handle_general_case(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item = ref_node->list.prev;
	struct list_head *first_item;
	u8 wreg;

	/* No-ops are handled automatically as they do not use registers. */
	if (!(ref_node->reg_mask & X86_REG_MASK(base)))
		return 0;
	
	wreg = kedr_choose_work_register(X86_REG_MASK_ALL, 
		ref_node->reg_mask, base);
	if (wreg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		return -EILSEQ;
	}
	
	/* adding code before the instruction sequence */
	item = kedr_mk_store_reg_to_spill_slot(wreg, base, item, 0, &err);
	first_item = item;
	item = kedr_mk_mov_reg_to_reg(base, wreg, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(base, wreg, item, 0, &err);
	
	/* adding code after the instruction sequence */
	item = &ref_node->list;
	item = kedr_mk_store_reg_to_spill_slot(base, wreg, item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(wreg, base, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(wreg, base, item, 0, &err);
	
	if (err == 0) {
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
		ref_node->last = 
			list_entry(item, struct kedr_ir_node, list);
	}
	else
		warn_fail(ref_node);
		
	return err;
}

// TODO
/* ====================================================================== */
