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
	
	/* adding code before the instruction */
	item = kedr_mk_store_reg_to_spill_slot(wreg, base, item, 0, &err);
	first_item = item;
	item = kedr_mk_mov_reg_to_reg(base, wreg, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(base, wreg, item, 0, &err);
	
	/* adding code after the instruction */
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
	else {
		warn_fail(ref_node);
	}
		
	return err;
}

/* ====================================================================== */
/* Transformation of the IR, phase 2 */
/* ====================================================================== */

/* A helper function that generates the instructions necessary to copy the
 * pointer to 'block_info' instance to <offset_info>(%base).
 *
 * On x86-32 this is done as follows:
 *	mov <block_info>, <offset_info>(%base)
 * 
 * On x86-64 this is done as follows:
 *	mov <block_info>, %rax
 *	mov %rax, <offset_info>(%base)
 * # Saving and restoring %rax is the caller's job.
 *
 * Similar to how kedr_mk_* functions behave, the function returns the last 
 * created item if successful. In case of an error, it also returns some 
 * valid item. 'err' is also handled according to the same rules that hold 
 * for kedr_mk_*() functions. */
static struct list_head *
mk_mov_block_info_ptr_to_ls(struct kedr_block_info *info, u8 base, 
	struct list_head *item, int *err)
{
	if (*err != 0)
		return item;
	
#ifdef CONFIG_X86_64
	item = kedr_mk_mov_imm64_to_rax((u64)info, item, 0, err);
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_AX, base, 
		offsetof(struct kedr_local_storage, info), item, 0, err);
#else /* x86-32 */
	item = kedr_mk_mov_value32_to_slot(
		(unsigned long)info, base,
		offsetof(struct kedr_local_storage, info), item, 0, err);
#endif
	return item;
}

/* A helper function that generates the instructions to call the given 
 * wrapper of some handler. It is assumed that no special operations are
 * needed here except passing the address of the local storage to the 
 * wrapper in %rax (hence "simple" in the name).
 * The rules concerning 'item' and 'err' are the same as for kedr_mk_*().
 *
 * Code:
 * 	push %rax
 *	mov %base, %rax
 *	call <wrapper_addr>
 *	pop %rax
 */
static struct list_head *
mk_call_wrapper_simple(unsigned long wrapper_addr, u8 base, 
	struct list_head *item, int *err)
{
	if (*err != 0)
		return item;
	
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, err);
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, err);
	item = kedr_mk_call_rel32(wrapper_addr, item, 0, err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, err);
	return item;
}

/* A helper function that generates the instructions to copy the pointer to 
 * a block_info instance to local_storage::info and then call the given 
 * wrapper of a handler. 
 * The rules concerning 'item' and 'err' are the same as for kedr_mk_*().
 *
 * Code:
 * 	push %rax
 *	<mov block_info to <offset_info>(%base)> # may use %rax
 *	mov %base, %rax
 *	call <wrapper_addr>
 *	pop %rax
 */
static struct list_head *
mk_call_wrapper_with_info(struct kedr_block_info *info, 
	unsigned long wrapper_addr, u8 base, struct list_head *item, 
	int *err)
{
	if (*err != 0)
		return item;
	
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, err);
	item = mk_mov_block_info_ptr_to_ls(info, base, item, err);
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, err);
	item = kedr_mk_call_rel32(wrapper_addr, item, 0, err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, err);
	return item;
}

/* Process the end of a common block that has no jumps out. It is enough to
 * save the pointer to the block_info instance in local_storage::info and
 * call kedr_on_common_block_end_wrapper(). */
int 
kedr_handle_block_end_no_jumps(struct kedr_ir_node *start_node, 
	struct kedr_ir_node *end_node, u8 base)
{
	int err = 0;
	
	BUG_ON(start_node->block_info == NULL);
	mk_call_wrapper_with_info(start_node->block_info,
		(unsigned long)&kedr_on_common_block_end_wrapper, base,
		&end_node->last->list, &err);
	
	if (err != 0) {
		pr_warning(KEDR_MSG_PREFIX 
		"Failed to add code at %pS, after the end of the block.\n",
			(void *)end_node->orig_addr);
	}
	return err;
}

/* Process the end of a common block that has jumps out of it.
 * [NB] These jumps are not necessarily performed. If one of such jumps is 
 * performed, 'dest_addr' field of the local storage will contain the 
 * address of the intended destination. If none of the jumps are performed,
 * 'dest_addr' will be 0. 
 * 
 * 'temp' field is used as a scratch area. kedr_on_common_block_end() 
 * does not change it. But it zeroes 'dest_addr' to make sure it is 0 
 * when each block begins.
 * 
 * [NB] We make use of the fact that the wrapper functions preserve all the
 * registers except %rax. So we can be sure %rdx will not be changed by
 * kedr_on_common_block_end_wrapper().
 *
 * [NB] We must preserve the values of flags in this code. 'test' may change
 * them, so pushf/popf are necessary.
 *
 * Code:
 * block_end:
 *	pushf
 * 	mov   %rdx, <offset_dx>(%base)
 *	mov   <offset_dest_addr>(%base), %rdx
 * 	push  %rax
 *	<mov start_node->block_info to <offset_info>(%base)> # may use %rax
 * 	mov   %base, %rax
 * 	call  <kedr_on_common_block_end_wrapper>  # [NB] zeroes 'dest_addr'.
 * 	pop   %rax
 * We need %rdx restored before the jump so we save the destination in a 
 * temporary.
 * 	mov   %rdx, <offset_temp>(%base) 
 * 	test  %rdx, %rdx
 * 	mov   <offset_dx>(%base), %rdx
 * 	jz    go_on
 *	popf
 *	jmp   *<offset_temp>(%base)
 * go_on:
 *	popf
 * next_block:
 */
int 
kedr_handle_block_end(struct kedr_ir_node *start_node, 
	struct kedr_ir_node *end_node, u8 base)
{
	struct kedr_ir_node *node;
	struct kedr_ir_node *node_jz;
	struct list_head *item = NULL;
	int err = 0;
	
	/* Create the first node of the sequence and place it after
	 * 'end_node->last', then create the node for 'jz'. 
	 * [NB] If the second allocation fails, the memory will be reclaimed
	 * anyway when the IR is destroyed. */
	node = kedr_ir_node_create();
	if (node == NULL)
		return -ENOMEM;
	list_add(&node->list, &end_node->last->list);
	
	node_jz = kedr_ir_node_create();
	if (node_jz == NULL)
		return -ENOMEM;
	
	item = kedr_mk_pushf(&node->list, 1, &err);
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_DX, base,
		item, 0, &err);
	item = kedr_mk_load_reg_from_mem(INAT_REG_CODE_DX, base, 
		(unsigned long)offsetof(struct kedr_local_storage, 
			dest_addr),
		item, 0, &err);
	item = mk_call_wrapper_with_info(start_node->block_info,
		(unsigned long)&kedr_on_common_block_end_wrapper, base,
		item, &err);
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_DX, base,
		(unsigned long)offsetof(struct kedr_local_storage, temp),
		item, 0, &err);
	item = kedr_mk_test_reg_reg(INAT_REG_CODE_DX, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(INAT_REG_CODE_DX, base, 
		item, 0, &err);
	
	list_add(&node_jz->list, item);	
	/* [NB] The destination node will be changed below. */
	item = kedr_mk_jcc(INAT_CC_Z, node, &node_jz->list, 1, &err);
	item = kedr_mk_popf(item, 0, &err);
	
	item = kedr_mk_jmp_offset_base(base, 
		(unsigned long)offsetof(struct kedr_local_storage, temp),
		item, 0, &err);
	item = kedr_mk_popf(item, 0, &err);

	if (err == 0) {
		node->last = 
			list_entry(item, struct kedr_ir_node, list);
		node_jz->dest_inner = node->last; 
		/* 'jz' jumps to the last 'popf' */
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
		"Failed to add code at %pS, after the end of the block.\n",
			(void *)end_node->orig_addr);
	}
	return err;
}

/* Process a block with a single operation, e.g. a locked update. The 
 * block is expected to have block_info instance associated with it.
 * Note that the blocks for memory barrier instructions not accessing 
 * memory are processed differently (see kedr_handle_barrier_other()).
 *
 * Part 1, apply it before the instruction sequence.
 * Code:
 *	push %rax
 *	<mov ref_node->block_info to <offset_info>(%base)> # may use %rax
 *	# The handlers expect their only argument to be in %rax
 *	mov %base, %rax
 *	call pre_wrapper
 *	pop %rax
 *
 * Part 2, apply it after the instruction sequence.
 * Code:
 * 	push %rax
 * 	mov %base, %rax
 * 	call post_wrapper
 *	pop %rax
 */
static int 
handle_single_op_block(struct kedr_ir_node *ref_node, u8 base, 
	unsigned long pre_wrapper, unsigned long post_wrapper)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item;
	
	/* adding code before the instruction sequence */
	item = mk_call_wrapper_with_info(ref_node->block_info, pre_wrapper,
		base, insert_after, &err);
	
	/* adding code after the instruction sequence */
	item = &ref_node->last->list;
	item = mk_call_wrapper_simple(post_wrapper, base, item, &err);
	
	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
		ref_node->last = 
			list_entry(item, struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	return err;
}

/* Process a block containing only a locked update operation. */
int 
kedr_handle_locked_op(struct kedr_ir_node *ref_node, u8 base)
{
	return handle_single_op_block(ref_node, base, 
		(unsigned long)&kedr_on_locked_op_pre_wrapper,
		(unsigned long)&kedr_on_locked_op_post_wrapper);
}

/* Process a block containing only an I/O operation accessing memory. */
int 
kedr_handle_io_mem_op(struct kedr_ir_node *ref_node, u8 base)
{
	return handle_single_op_block(ref_node, base, 
		(unsigned long)&kedr_on_io_mem_op_pre_wrapper,
		(unsigned long)&kedr_on_io_mem_op_post_wrapper);
}

/* Process a block containing only one operation, a memory barrier not 
 * accessing memory. 
 * 
 * Part 1, apply it before the instruction sequence.
 * Code:
 * 	push %rax
 *	# "MOV imm8, mem" because <barrier_type> should fit in one byte.
 * 	mov <barrier_type>, <offset_temp>(%base)
 *	(x86-32) mov <pc>, <offset_temp1>(%base)
 *	# Same opcode but the insn also performs sign-extension on x86-64
 *	(x86-64) mov <lower_32_bits_of_pc>, <offset_temp1>(%base)
 *	mov %base, %rax
 *	call kedr_on_barrier_pre_wrapper
 *	pop %rax
 *
 * Part 2, apply it after the instruction sequence.
 * Code:
 *	# Neither the pre handler nor the instruction sequence itself change
 *	# the fields 'temp' and 'temp1' of the local storage. So they have 
 *	# the needed values already.
 *	push %rax
 *	mov %base, %rax
 *	call kedr_on_barrier_post_wrapper
 *	pop %rax
 */
int 
kedr_handle_barrier_other(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item = ref_node->first->list.prev;
	struct list_head *first_item;
	
	/* adding code before the instruction sequence */
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
	first_item = item;
	
	item = kedr_mk_mov_value8_to_slot((u8)ref_node->barrier_type, base,
		offsetof(struct kedr_local_storage, temp), item, 0, &err);
	item = kedr_mk_mov_value32_to_slot((u32)ref_node->orig_addr, base,
		offsetof(struct kedr_local_storage, temp1), item, 0, &err);
	
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_on_barrier_pre_wrapper, item, 0, &err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	
	/* adding code after the instruction sequence */
	item = &ref_node->last->list;
	item = mk_call_wrapper_simple(
		(unsigned long)&kedr_on_barrier_post_wrapper, base, item, 
		&err);
	
	if (err == 0) {
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
		ref_node->last = 
			list_entry(item, struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	return err;
}

/* Process a direct jump (call/jmp near, jcc near) from a block to 
 * another block. We need to make sure a handler function for the block end
 * is called before the execution of another block begins.
 *
 * insn: <op> <disp32>
 * 
 * During the instrumentation, we know the destination node of the jump
 * ('node->dest_inner') but the exact address is not known until the end. 
 * Therefore, we will need a kind of "relocation" that will replace the 
 * 32-bit value (<val32>) in 'mov' with the lower 32 bits of 
 *   SignExt(<val32>) + <Address_of_mov> + <Length_of_mov>.
 * That is, <dest32> is the value in the lower 32 bits of the destination 
 * address (automatic sign extension will give the full address on x86-64).
 * In turn, <val32> is calculated during the code generation phase before
 * relocation. It is the offset of the jump destination from the end of 
 * that 'mov' instruction.
 * 
 * Code for jmp <disp32>:
 *	mov <dest32>, <offset_dest_addr>(%base)
 * 	jmp <disp_end>
 *
 * Code for call <disp32>:
 *	mov <dest32>, <offset_dest_addr>(%base)
 * 	call <disp_end>
 * 
 * Code for jcc <disp32>:
 *	j<not cc> go_on
 *	mov <dest32>, <offset_dest_addr>(%base)
 * 	jmp <disp_end>
 * go_on:
 *	# NB> <dest_addr> remains 0 in the storage if the jump is not taken.
 * 
 * <disp_end> is a displacement of the position just past the end of what
 * the last instruction of the block transforms to. A "block_end" handler
 * will be placed there that will dispatch the jump properly.
 * Set 'dest_inner' to the last node of the block for the insn above.
 * A flag should have been already set in the node to indicate it is a 
 * jump out ouf a block. So <disp_end> will be set properly during code
 * generation as the displacement of the node just after 'end_node->last'.*/
int
kedr_handle_jump_out_of_block(struct kedr_ir_node *ref_node, 	
	struct kedr_ir_node *end_node, u8 base)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct kedr_ir_node *first = NULL;
	struct kedr_ir_node *node_jnotcc = NULL;
	struct kedr_ir_node *node_mov = NULL;
	struct insn *insn = &ref_node->insn;
	u8 opcode;
	u8 cc;
	
	BUG_ON(ref_node->jump_past_last == 0);
	
	opcode = insn->opcode.bytes[0];
	
	/* First, create and add the node for 'mov'. */
	node_mov = kedr_ir_node_create();
	if (node_mov == NULL)
		return -ENOMEM;
	list_add(&node_mov->list, insert_after);
	kedr_mk_mov_value32_to_slot(0, base, 
		(u32)offsetof(struct kedr_local_storage, dest_addr),
		&node_mov->list, 1, &err);
		
	/* Set 'dest_inner' for the node with 'mov' to be able to properly 
	 * relocate imm32 there later. */
	node_mov->dest_inner = ref_node->dest_inner;
	node_mov->needs_addr32_reloc = 1;
	first = node_mov;
	
	/* If it was a conditional jump originally, place 'j<not cc>' before
	 * 'mov'. */
	if (opcode == 0x0f && (insn->opcode.bytes[1] & 0xf0) == 0x80) {
		/* Prepare the condition code for the inverted condition. */
		cc = insn->opcode.bytes[1] & 0x0f;
		cc ^= 1;
		
		node_jnotcc = kedr_ir_node_create();
		if (node_jnotcc == NULL)
			return -ENOMEM;
		list_add(&node_jnotcc->list, insert_after);
		kedr_mk_jcc(cc, ref_node, &node_jnotcc->list, 1, &err);
		node_jnotcc->jump_past_last = 1;
		
		first = node_jnotcc;
	}
	
	/* Replace the original jump with a call/jump to the end of the 
	 * block. */
	kedr_mk_call_jmp_to_inner(end_node, (opcode != KEDR_OP_CALL_REL32), 
		&ref_node->list, 1, &err);
	/* 'jump_past_last' remains 1, which is what we need. */
	
	if (err == 0)
		ref_node->first = first;
	else
		warn_fail(ref_node);
	return err;
}
/* ====================================================================== */

/* The offset of values[_index] in struct kedr_local_storage. */
#define KEDR_OFFSET_VALUES_N(_index) \
	((unsigned int)offsetof(struct kedr_local_storage, values) + \
	(unsigned int)((_index) * sizeof(unsigned long)))

/* A helper function that generates the code given below to record the 
 * memory access from the instruction of type E or M in the given node. 
 * 
 * There are 2 cases: 
 * - %base is not used in the memory addressing expression <expr>; 
 * - %base is used there. 
 * %wreg is selected among the registers not used in <expr>, it should be
 * different from %base too.
 *
 * <offset_values[nval]> - offset of values[nval] in the local storage.
 *
 * Code: 
 * 
 * Case 1: %base is not used in <expr>
 *	mov  %wreg, <offset_wreg>(%base)
 *	lea  <expr>, %wreg
 * The following part is the same in both cases:
 *	mov  %wreg, <offset_values[nval]>(%base)
 * 	mov  <offset_wreg>(%base), %wreg
 *
 * ---------------------------------------------------
 * Case 2: %base is used in <expr>. 
 *	mov  %wreg, <offset_wreg>(%base)
 * 	mov  %base, %wreg
 * 	mov  <offset_base>(%wreg), %base
 *	lea  <expr>, %base
 *	xchg %base, %wreg
 * The following part is the same in both cases:
 *	mov  %wreg, <offset_values[nval]>(%base)
 * 	mov  <offset_wreg>(%base), %wreg 
 * 
 * The rules concerning 'item' and 'err' are the same as for kedr_mk_*(). */
static struct list_head *
mk_record_access_common(struct kedr_ir_node *node, u8 base, 
	unsigned int nval, struct list_head *item, int *err)
{
	u8 wreg;
	unsigned int expr_reg_mask;
	int base_is_used;
	
	if (*err != 0)
		return item;
	
	expr_reg_mask = insn_reg_mask_for_expr(&node->insn);
	base_is_used = (expr_reg_mask & X86_REG_MASK(base));
	
	wreg = kedr_choose_work_register(X86_REG_MASK_ALL,
		(expr_reg_mask | X86_REG_MASK(INAT_REG_CODE_SP)), 
		base);
	if (wreg == KEDR_REG_NONE) {
		warn_no_wreg(node, base);
		*err = -EILSEQ;
		return item;
	}
	
	item = kedr_mk_store_reg_to_spill_slot(wreg, base, item, 0, err);
	
	if (base_is_used) {
		item = kedr_mk_mov_reg_to_reg(base, wreg, item, 0, err);
		item = kedr_mk_load_reg_from_spill_slot(base, wreg, item, 0,
			err);
		item = kedr_mk_lea_expr_reg(node, base, item, 0, err);
		item = kedr_mk_xchg_reg_reg(base, wreg, item, 0, err);
	}
	else {
		item = kedr_mk_lea_expr_reg(node, wreg, item, 0, err);
	}
	
	item = kedr_mk_store_reg_to_mem(wreg, base, 
		KEDR_OFFSET_VALUES_N(nval), item, 0, err);
	
	item = kedr_mk_load_reg_from_spill_slot(wreg, base, item, 0, err);
	return item;
}

/* Process memory accesses for the following instructions:
 * 	SETcc and CMOVcc
 *
 * Apply this before the instruction sequence.
 * 
 * Code:
 *	j<not cc> go_over
 * 	... # see mk_record_access_common()
 * go_over:
 */
int
kedr_handle_setcc_cmovcc(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item;
	struct insn *insn = &ref_node->insn;
	struct kedr_ir_node *node_jcc;
	u8 cc;
	
	/* Obtain the condition code from the last opcode byte, then invert
	 * the least significant bit to invert the condition (see Intel's 
	 * manual vol 2B, "B.1.4.7 Condition Test (tttn) Field"). */
	BUG_ON(insn->opcode.nbytes == 0);
	cc = insn->opcode.bytes[insn->opcode.nbytes - 1] & 0x0f;
	cc ^= 1;
	
	node_jcc = kedr_ir_node_create();
	if (node_jcc == NULL)
		return -ENOMEM;
	node_jcc->jump_past_last = 1;
	list_add(&node_jcc->list, insert_after);
	
	/* A jump to the node following 'node_jcc->last'.
	 * [NB] We cannot make ref_node the destination of this jump because 
	 * our system will later set the destination to 'ref_node->first'
	 * instead. That is, the jump will lead to itself in that case. */
	item = kedr_mk_jcc(cc, node_jcc, &node_jcc->list, 1, &err);
	item = mk_record_access_common(ref_node, base, nval, item, &err);
	
	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
		node_jcc->last = list_entry(item, struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	
	return err;
}

/* Processing memory accesses for CMPXCHG*.
 *
 * <set_bit_N> is a 32-bit value where only the bit at position N is 1.
 * When written to the appropriate slot in the storage, it is sign-extended
 * but only the lower 32 bits are used when analysing.
 * N is the number of the memory access in the block.
 *
 * [NB] Use "or" rather than "mov" to update the mask: other instructions 
 * of this kind in the block may do the same.
 * 
 * Part 1: record read (happens always, should be taken into account in 
 * read_mask). Apply this before the instruction sequence.
 * Code:
 *	... # same as "E-general"
 * 
 * Part 2: update the data if write happens. Apply this after the 
 * instruction sequence.
 * If ZF is 0, it is read operation again, nothing to do.
 * Code: 
 *	jnz   go_on
 *	pushf
 *	or  <set_bit_N>, <offset_write_mask>(%base)
 *	popf
 * go_on:
 */
static int
handle_cmpxchg_impl(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item = NULL;
	struct kedr_ir_node *node_jnz;
	
	/* Create the node for 'jnz' (to be filled later) */
	node_jnz = kedr_ir_node_create();
	if (node_jnz == NULL)
		return -ENOMEM;
	
	mk_record_access_common(ref_node, base, nval, insert_after, &err);
	list_add(&node_jnz->list, &ref_node->last->list);
	
	/* Make sure the jump will lead to the node following the 
	 * instruction sequence created in this function. Note that the 
	 * instruction sequence may expand further in the later stages of 
	 * processing, e.g. when instrumenting LOCK CMPXCHG*. We need to
	 * make sure the jump leads to the node following the node for POPF
	 * created here, hence we set 'node_jnz->last' below. */
	item = kedr_mk_jcc(INAT_CC_NZ, node_jnz, &node_jnz->list, 1, &err);
	node_jnz->jump_past_last = 1;
	
	item = kedr_mk_pushf(item, 0, &err);
	item = kedr_mk_or_value32_to_slot(((u32)1 << num), base, 
		offsetof(struct kedr_local_storage, write_mask), item, 0, 
		&err);
	item = kedr_mk_popf(item, 0, &err);
	
	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
		ref_node->last = list_entry(item, struct kedr_ir_node, list);
		node_jnz->last = list_entry(item, struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	return err;
}

int
kedr_handle_cmpxchg(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval)
{
	return handle_cmpxchg_impl(ref_node, base, num, nval);
}

int
kedr_handle_cmpxchg8b_16b(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval)
{
	return handle_cmpxchg_impl(ref_node, base, num, nval);
}

int
kedr_handle_type_e_and_m(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	
	mk_record_access_common(ref_node, base, nval, insert_after, &err);
	
	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	return err;
}

/* Processing memory access for XLAT instruction. 
 * Apply this before the instruction sequence.
 *
 * There are 2 cases: 
 * 	1) %base is %rbx - the instructions that should be used in this 
 * case only are enclosed in [].
 * 	2) %base is not %rbx  - the instructions that should be used in 
 * this case only are enclosed in {}. 
 * The instructions that are not in brackets are the same in both cases.
 *
 * Note that both on x86-32 and on x86-64 the instructions listed below 
 * deal with the full-sized registers (except %al).
 * To simplify things a bit, we also assume that XLAT itself uses full-sized
 * register as a base, that is, %ebx on x86-32 and %rbx on x86-64. In the 
 * latter case, this means that REX.W must be present. It seems to be 
 * unlikely that %ebx is used to contain the address of the XLAT table 
 * on x86-64 rather than %rbx and the values of %rbx and (extended) %ebx
 * are not the same.
 *
 * [NB] MOVZBL is sometimes used as another mnemonic for the variant of 
 * MOVZX we use below.
 *
 * Code:
 *	mov  %rax, <offset_ax>(%base)
 * On entry, %al contains an unsigned index to the table XLAT uses.
 *	movzx %al, %rax  # zero-extend the unsigned index...
 * ...and add the original value of %rbx to it:
 *	pushf	# 'add' affects flags, so we need to preserve them
 *	[add <offset_bx>(%base), %rax]	# use this one if %base is %rbx...
 *	{add %rbx, %rax}		# ...or this one if not
 * 	popf
 * Now %rax contains the address of the byte to be accessed by XLAT.
 *	mov  %rax, <offset_values[nval]>(%base)
 * 	mov  <offset_ax>(%base), %rax	# restore %rax
 */
int
kedr_handle_xlat(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item;
	
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_AX, base,
		insert_after, 0, &err);
	item = kedr_mk_movzx_al_ax(item, 0, &err);
	item = kedr_mk_pushf(item, 0, &err);
	
	if (base == INAT_REG_CODE_BX)
		item = kedr_mk_add_slot_bx_to_ax(base, item, 0, &err);
	else
		item = kedr_mk_add_bx_to_ax(item, 0, &err);
	
	item = kedr_mk_popf(item, 0, &err);
	
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_AX, base, 
		KEDR_OFFSET_VALUES_N(nval), item, 0, &err);
	
	item = kedr_mk_load_reg_from_spill_slot(INAT_REG_CODE_AX, base,
		item, 0, &err);

	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	return err;
}

/* Processing memory accesses for direct memory offset MOVs (opcodes A0-A3).
 * 
 * Apply this before the instruction sequence.
 * Code for x86-32:
 *	mov  <addr>, <offset_values[nval]>(%base)
 *
 * Code for x86-64:
 *	push %rax
 *	mov <addr>, %rax
 *	mov %rax, <offset_values[nval]>(%base)
 *	pop %rax
 */
int
kedr_handle_direct_offset_mov(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval)
{
	int err = 0;
	struct insn *insn = &ref_node->insn;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item;
	
	item = insert_after;
	
#ifdef CONFIG_X86_64
	{
		u64 addr64 = ((u64)insn->moffset2.value << 32) | 
			(u64)insn->moffset1.value; 
		item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
		item = kedr_mk_mov_imm64_to_rax(addr64, item, 0, &err);
		item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_AX, base,
			KEDR_OFFSET_VALUES_N(nval), item, 0, &err);
		item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	}
#else /* x86-32 */
	item = kedr_mk_mov_value32_to_slot((u32)insn->moffset1.value, base, 
		KEDR_OFFSET_VALUES_N(nval), item, 0, &err);
#endif

	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	return err;
}

/* Processing memory accesses for the instruction with addressing methods 
 * "X" and "Y" (but not "XY"). The method is specified via 'amethod'.
 * [NB] REP* prefixes are taken into account automatically.
 *
 * %key_reg is %rsi for "X" and %rdi for "Y".
 * %wreg is a register not used by the instruction and different 
 * from %base as usual. It must not be %rsp either.
 * %treg is a register different from %base, %wreg, %rsp and %key_reg.
 *
 * Note that the policy used to select %base guarantees that %base is not
 * used by the instructions with X and Y addressing methods. %base is 
 * selected from non-scratch registers, so it cannot be %ax, %cx or %dx used
 * by some of such instructions. On x86-64, it can be neither %rsi nor %rdi 
 * for the same reason. On x86-32, the policy requires that %esi and %edi 
 * must not be chosen as %base for a function if there are X- or 
 * Y-instructions in the function. To sum up, we can rely on the fact that 
 * the instruction does not use %base.
 *
 * Part 1, apply it before the instruction.
 * Code:
 *	mov  %wreg, <offset_wreg>(%base)
 * 	mov  %key_reg, %wreg	# start position
 * ----------------------------------------------
 *
 * Part 2, apply it after the instruction.
 * Now %key_reg is the position past the end by the size of the element 
 * (S: 1, 2, 4 or 8). 
 * [NB] The size of the element was saved in block_info on the previous 
 * stages of the instrumentation.
 *
 * We need to determine the beginning and the length of the accessed
 * memory area taking the direction of processing in to account.
 * Code:
 * 	mov  %treg, <offset_treg>(%base)
 *	pushf	
 *	mov  %key_reg, %treg 	# treg - position past the end by S
 *	sub  %wreg, %treg	# treg -= wreg => treg := +/- length
 *	jz out			# length == 0, nothing has been processed
 *	ja record_access	# common case: forward processing
 * The data have been processed backwards.
 *	mov  %key_reg, %wreg	# set the real start: new %key_reg + S
 *	add  <S>, %wreg		# S<=8 => 8 bits are more than enough
 * 	neg  %treg		# treg = -treg; now treg == length
 *
 * record_access:
 *
 * No matter in which direction the data processing was made, %wreg is now 
 * the start address of the accessed memory block and %treg is the length of
 * the block in bytes. [%wreg, %wreg + %treg)
 *	mov  %wreg, <offset_values[nval]>(%base)
 *	mov  %treg, <offset_values[nval+1]>(%base)
 * out:
 *	popf
 *	mov  <offset_treg>(%base), %treg
 *	mov  <offset_wreg>(%base), %wreg 
 */
static int 
handle_type_x_and_y_impl(struct kedr_ir_node *ref_node, 
	struct kedr_block_info *info, u8 base, 
	unsigned int num, unsigned int nval, u8 amethod)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item = NULL;
	struct kedr_ir_node *node_record_access = NULL;
	struct kedr_ir_node *node_out = NULL;
	
	u8 wreg;
	u8 treg;
	u8 key_reg = (amethod == INAT_AMETHOD_X ? 
		INAT_REG_CODE_SI :
		INAT_REG_CODE_DI);
	
	u8 sz = (u8)info->events[num].size;
	
	wreg = kedr_choose_work_register(X86_REG_MASK_ALL,
		(ref_node->reg_mask | X86_REG_MASK(INAT_REG_CODE_SP)), 
		base);
	if (wreg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		return -EILSEQ;
	}
	
	treg = kedr_choose_work_register(X86_REG_MASK_ALL,
		(X86_REG_MASK(wreg) | X86_REG_MASK(key_reg) | 
			X86_REG_MASK(INAT_REG_CODE_SP)), 
		base);
	if (treg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		return -EILSEQ;
	}
	
	node_record_access = kedr_ir_node_create();
	if (node_record_access == NULL)
		return -ENOMEM;
	node_out = kedr_ir_node_create();
	if (node_out == NULL) {
		kedr_ir_node_destroy(node_record_access);
		return -ENOMEM;
	}
	
	/* Part 1 - added before the instruction */
	item = kedr_mk_store_reg_to_spill_slot(wreg, base, insert_after,
		0, &err);
	item = kedr_mk_mov_reg_to_reg(key_reg, wreg, item, 0, &err);
	
	/* Part 2 - added after the instruction */
	item = kedr_mk_store_reg_to_spill_slot(treg, base, 
		&ref_node->last->list, 0, &err);
	item = kedr_mk_pushf(item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(key_reg, treg, item, 0, &err);
	item = kedr_mk_sub_reg_reg(wreg, treg, item, 0, &err);
	item = kedr_mk_jcc(INAT_CC_Z, node_out, item, 0, &err);
	item = kedr_mk_jcc(INAT_CC_A, node_record_access, item, 0, &err);
	
	item = kedr_mk_mov_reg_to_reg(key_reg, wreg, item, 0, &err);
	item = kedr_mk_add_value8_to_reg(sz, wreg, item, 0, &err);
	item = kedr_mk_neg_reg(treg, item, 0, &err);
	
	kedr_mk_store_reg_to_mem(wreg, base, KEDR_OFFSET_VALUES_N(nval), 
		&node_record_access->list, 1, &err);
	list_add(&node_record_access->list, item);
	item = &node_record_access->list;
		
	item = kedr_mk_store_reg_to_mem(treg, base,
		KEDR_OFFSET_VALUES_N(nval + 1), item, 0, &err);
	
	kedr_mk_popf(&node_out->list, 1, &err);
	list_add(&node_out->list, item);
	item = &node_out->list;
	
	item = kedr_mk_load_reg_from_spill_slot(treg, base, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(wreg, base, item, 0, &err);
	
	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
		ref_node->last = list_entry(item, struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	return err;
}

int
kedr_handle_type_x(struct kedr_ir_node *ref_node, 
	struct kedr_block_info *info, u8 base, 
	unsigned int num, unsigned int nval)
{
	return handle_type_x_and_y_impl(ref_node, info, base, num, nval, 
		INAT_AMETHOD_X);
}

int
kedr_handle_type_y(struct kedr_ir_node *ref_node, 
	struct kedr_block_info *info, u8 base, 
	unsigned int num, unsigned int nval)
{
	return handle_type_x_and_y_impl(ref_node, info, base, num, nval, 
		INAT_AMETHOD_Y);
}

/* Processing memory accesses for the instruction with "X" and "Y" addressing 
 * modes (when both modes are used for an instruction):
 * MOVS, CMPS
 * 
 * [NB] REP* prefixes are taken into account automatically.
 *
 * We make use of the fact that %rax and %rdx are not used by MOVS and CMPS.
 * These registers are used as work registers.
 * After the instruction completes, we may use %rcx too, and it is
 * convenient to make it the third work register we need. 
 *
 * Part 1, apply it before the instruction
 * Code:
 *	mov  %rax, <offset_ax>(%base)
 *	mov  %rdx, <offset_dx>(%base)
 * 	mov  %rsi, %rax
 * 	mov  %rdi, %rdx
 * ----------------------------------------
 *
 *
 * Part 2, apply it after the instruction
 * Code:
 * 	mov  %rcx, <offset_cx>(%base)
 *	pushfq
 * 	mov  %rsi, %rcx		# rcx - position past the end by S
 *	sub  %rax, %rcx		# rcx -= (old rsi) => wreg := +/- length
 *	jz out			# length == 0, nothing has been processed
 *	ja record_access	# common case: forward processing
 * The data have been processed backwards (rcx is negative).
 *	mov  %rsi, %rax		# set the real start position: new %rsi + S
 *	add  <S>, %rax		# <S> is 1, 2, 4, or 8 => 8 bits is enough
 *	mov  %rdi, %rdx		# ... same for %rdi
 *	add  <S>, %rdx
 * 	neg  %rcx		# rcx = -rcx; now rcx == length
 *
 * record_access:
 *
 * Record accesses to [%rax, %rax + %rcx) and [%rdx, %rdx + %rcx)
 *	mov  %rax, <offset_values[nval]>(%base)
 *	mov  %rcx, <offset_values[nval+1]>(%base)
 *	mov  %rdx, <offset_values[nval+2]>(%base)
 *	mov  %rcx, <offset_values[nval+3]>(%base)
 * out:
 *	popfq
 *	mov  <offset_cx>(%base), %rcx
 *	mov  <offset_dx>(%base), %rdx
 *	mov  <offset_ax>(%base), %rax
 */
int
kedr_handle_type_xy(struct kedr_ir_node *ref_node, 
	struct kedr_block_info *info, u8 base, 
	unsigned int num, unsigned int nval)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item = NULL;
	struct kedr_ir_node *node_record_access = NULL;
	struct kedr_ir_node *node_out = NULL;
	
	u8 sz = (u8)info->events[num].size;
	
	node_record_access = kedr_ir_node_create();
	if (node_record_access == NULL)
		return -ENOMEM;
	node_out = kedr_ir_node_create();
	if (node_out == NULL) {
		kedr_ir_node_destroy(node_record_access);
		return -ENOMEM;
	}
	
	/* Part 1 - added before the instruction */
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_AX, base, 
		insert_after, 0, &err);
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_DX, base,
		item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(INAT_REG_CODE_SI, INAT_REG_CODE_AX, 
		item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(INAT_REG_CODE_DI, INAT_REG_CODE_DX, 
		item, 0, &err);
	
	/* Part 2 - added after the instruction */
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_CX, base, 
		&ref_node->last->list, 0, &err);
	item = kedr_mk_pushf(item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(INAT_REG_CODE_SI, INAT_REG_CODE_CX,
		item, 0, &err);
	item = kedr_mk_sub_reg_reg(INAT_REG_CODE_AX, INAT_REG_CODE_CX, 
		item, 0, &err);
	item = kedr_mk_jcc(INAT_CC_Z, node_out, item, 0, &err);
	item = kedr_mk_jcc(INAT_CC_A, node_record_access, item, 0, &err);
	
	item = kedr_mk_mov_reg_to_reg(INAT_REG_CODE_SI, INAT_REG_CODE_AX,
		item, 0, &err);
	item = kedr_mk_add_value8_to_reg((u8)sz, INAT_REG_CODE_AX, item, 0,
		&err);
	item = kedr_mk_mov_reg_to_reg(INAT_REG_CODE_DI, INAT_REG_CODE_DX,
		item, 0, &err);
	item = kedr_mk_add_value8_to_reg((u8)sz, INAT_REG_CODE_DX, item, 0,
		&err);
	item = kedr_mk_neg_reg(INAT_REG_CODE_CX, item, 0, &err);
	
	/* Record accesses to [%rax, %rax + %rcx) and [%rdx, %rdx + %rcx) */
	kedr_mk_store_reg_to_mem(INAT_REG_CODE_AX, base, 
		KEDR_OFFSET_VALUES_N(nval), &node_record_access->list, 1, 
		&err);
	list_add(&node_record_access->list, item);
	item = &node_record_access->list;
	
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_CX, base,
		KEDR_OFFSET_VALUES_N(nval + 1), item, 0, &err);
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_DX, base, 
		KEDR_OFFSET_VALUES_N(nval + 2), item, 0, &err);
	item = kedr_mk_store_reg_to_mem(INAT_REG_CODE_CX, base,
		KEDR_OFFSET_VALUES_N(nval + 3), item, 0, &err);

	/* out: */
	kedr_mk_popf(&node_out->list, 1, &err);
	list_add(&node_out->list, item);
	item = &node_out->list;
	
	item = kedr_mk_load_reg_from_spill_slot(INAT_REG_CODE_CX, base, 
		item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(INAT_REG_CODE_DX, base, 
		item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(INAT_REG_CODE_AX, base, 
		item, 0, &err);
	
	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
		ref_node->last = list_entry(item, struct kedr_ir_node, list);
	}
	else {
		warn_fail(ref_node);
	}
	return err;
}
/* ====================================================================== */
