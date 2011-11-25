/* ir_handlers.h - the functions to update the appropriate parts of the IR
 * during the instrumentation. */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>

#include "ifunc.h"
#include "primary_storage.h"
#include "ir.h"
#include "code_gen.h"
#include "operations.h"
#include "util.h"

//<> For debugging only
#include "debug_util.h"
extern char *target_function;
extern const char *func_name;
extern const struct kedr_ifunc *dbg_ifunc;
//<>
/* ====================================================================== */

/* At the phase 2, if a handler adds nodes before or after the reference 
 * node 'ref_node', it should actually place the nodes before 
 * 'ref_node->first' or after 'ref_node->last', respectively. The handler is
 * responsible for updating these  fields after that. */
/* ====================================================================== */

static void
warn_no_wreg(struct kedr_ir_node *node, u8 base)
{
	pr_err("[sample] No registers left that can be chosen as "
		"a work register to handle the instruction at %pS "
		"(%%base: %u). Unable to instrument the function.\n",
		(void *)node->orig_addr, (unsigned int)base);
}

static void
warn_fail(struct kedr_ir_node *node)
{
	pr_err("[sample] Failed to instrument the instruction at %pS.\n",
		(void *)node->orig_addr);
}
/* ====================================================================== */

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
	
	item = kedr_mk_mov_expr_reg(&ref_node->insn, wreg, item, 0, err);
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
 * function, that allocates the primary storage and sets the base register.
 *   'ir' - the IR of the function.
 * 
 * Code:
 *	push  %rax
 *	mov   <orig_func_addr32>, %rax	# sign-extended on x86-64
 *	call  <kedr_process_function_entry_wrapper>
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
		(unsigned long)&kedr_process_function_entry_wrapper, 
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
		/* Fill the node for 'jnz properly'. All the previous 
		 * kedr_mk_* operations have completed successfully, so
		 * 'go_on_item' must be the 'list' field of a real node. */
		struct kedr_ir_node *go_on_node = 
			list_entry(go_on_item, struct kedr_ir_node, list);
		kedr_mk_jcc(INAT_CC_NZ, go_on_node, &jnz_node->list, 
			1 /* do it in place */, &err);
	}
	
	if (err != 0) {
		pr_err("[sample] Failed to instrument the entry of "
			"the function %s().\n", func->name);
	}
	return err;
}

/* Handling of an exit from the function (except an indirect jump which is 
 * processed separately): ret, direct jump outside, ... 
 * 
 * Code to insert before the instruction sequence:
 * 	push  %rax
 * 	mov   %base, %rax
 * 	mov   <offset_base>(%rax), %base
 * 	call  <kedr_process_function_exit_wrapper>
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
		(unsigned long)&kedr_process_function_exit_wrapper,
		item, 0, &err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	
	if (err == 0)
		ref_node->first = 
			list_entry(first_item, struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
		
	return err;
}

/* Processing the following instruction: 
 * 	call near indirect (call *<expr>)
 * 
 * Two goals here:
 * - if the instruction uses %base, handle this appropriately;
 * - allow for the replacement of indirect calls in runtime.
 *
 * The instructions in square brackets are used only if the original 
 * instruction uses the base register.
 *
 * [NB] %wreg is a register not used by the call. That is, it is callee-save 
 * register among other things and will be preserved by the call. It is not 
 * used for parameter passing either, so it is safe to use 'call *%wreg' 
 * (we assume it is a function call, not some "dirty trick").
 * %treg - a register not used in <expr>.
 * %base, %wreg and %treg are different registers, as usual.
 *
 * Code:
 *      mov   %wreg, <offset_wreg>(%base) 
 * 	[mov   %treg, <offset_treg>(%base)] 
 *	[mov   %base, %treg]
 *	[mov   <offset_base>(%treg), %base]
 * 	mov   (<expr>), %<wreg>
 *	[mov   %treg, %base]
 *	[mov   <offset_treg>(%base), %treg]
 * 	push  %rax
 *	mov   %wreg, %rax
 *	call  kedr_lookup_replacement_wrapper
 * 	mov   %rax, %wreg
 *	pop   %rax
 *	call  *%wreg	# this will replace the original insn
 * 	mov   <offset_wreg>(%base), %wreg
 */
int
kedr_handle_call_near_indirect(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item = ref_node->list.prev;
	struct list_head *first_item;
	u8 wreg;
	
	wreg = kedr_choose_work_register(X86_REG_MASK_NON_SCRATCH, 
		(ref_node->reg_mask | X86_REG_MASK(INAT_REG_CODE_SP)), 
		base);
	if (wreg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		return -EILSEQ;
	}
	item = kedr_mk_store_reg_to_spill_slot(wreg, base, item, 0, &err);
	first_item = item;
	
	item = mk_eval_addr_to_reg(ref_node, base, wreg, item, &err);
	
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(wreg, INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_lookup_replacement_wrapper,
		item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(INAT_REG_CODE_AX, wreg, item, 0, &err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	
	/* replace the original instruction with 'call *%wreg' */
	item = kedr_mk_call_reg(wreg, &ref_node->list, 1, &err);
	
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
	item = kedr_mk_mov_expr_reg(&ref_node->insn, wreg, item, 0, &err);
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

/* Processing of the indirect near jumps (jmp *<expr>) that transfer control 
 * outside of the instrumented instance of the function.
 * 
 * The goals:
 * - if the instruction uses %base, handle this appropriately;
 * - allow for the replacement of indirect calls in runtime;
 *
 * The instructions in square brackets are used only if the original 
 * instruction uses the base register.
 *
 * %wreg - a non-scratch register not used by the jump and not %rsp.
 * %treg - a register not used in <expr>.
 * %base, %wreg and %treg are different registers, as usual.
 * 
 * Code:
 *      mov   %wreg, <offset_wreg>(%base) 
 * 	[mov   %treg, <offset_treg>(%base)] 
 *	[mov   %base, %treg]
 *	[mov   <offset_base>(%treg), %base]
 * 	mov   (<expr>), %wreg
 *	[mov   %treg, %base]
 *	[mov   <offset_treg>(%base), %treg]
 * 	push  %rax
 *	mov   %wreg, %rax
 *	call  <kedr_lookup_replacement_wrapper>
 * The original value of %rax is now at the top of the stack, load it to 
 * %rax again and place the destination address instead, then save the 
 * original value of %rax on stack again: we need %rax for the call to 
 * kedr_process_function_exit().
 *	xchg  %rax, (%rsp)
 * 	push  %rax
 * 	mov   %base, %rax
 * 	mov   <offset_wreg>(%rax), %wreg
 * 	mov   <offset_base>(%rax), %base
 * 	call  <kedr_process_function_exit_wrapper>
 *	pop   %rax
 * Now the destination address lies at the top of the stack. 'ret' will 
 * extract it from there and will perform a jump there. All the registers
 * have their unbiased values before that, which is what we need.
 *	ret	# this will replace the original instruction
 * We place a "trap" here just in case it is a weird jump that returns (e.g. 
 * push <ret_addr>; jump <func>; ...). 
 * kedr_warn_unreachable_wrapper() is called here with the address of the 
 * original near indirect jump as the only argument. The function may output
 * messages, etc., but must not use BUG(): to make sure the execution of
 * this code will not continue, we place 'ud2' below manually.
 *	push  %rax
 *	mov  <address_of_the_orig_insn>, %rax # sign-extension is OK
 *	call <kedr_warn_unreachable_wrapper>
 *	pop   %rax
 *	ud2
 */
int
kedr_handle_jmp_indirect_out(struct kedr_ir_node *ref_node, u8 base)
{
	int err = 0;
	struct list_head *item;
	struct list_head *first_item;
	u8 wreg;
	
	/* This handler operates in the instruction itself, so it must be
	 * called before any other handler for this instruction. */
	BUG_ON(ref_node->first != ref_node || ref_node->last != ref_node);
	
	wreg = kedr_choose_work_register(X86_REG_MASK_NON_SCRATCH, 
		(ref_node->reg_mask | X86_REG_MASK(INAT_REG_CODE_SP)), 
		base);
	if (wreg == KEDR_REG_NONE) {
		warn_no_wreg(ref_node, base);
		return -EILSEQ;
	}
	
	item = ref_node->list.prev;
	item = kedr_mk_store_reg_to_spill_slot(wreg, base, item, 0, &err);
	first_item = item;
	
	item = mk_eval_addr_to_reg(ref_node, base, wreg, item, &err);
	
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(wreg, INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_lookup_replacement_wrapper,
		item, 0, &err);
	item = kedr_mk_xchg_ax_stack_top(item, 0, &err);
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, &err);
	
	item = kedr_mk_load_reg_from_spill_slot(wreg, INAT_REG_CODE_AX, 
		item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(base, INAT_REG_CODE_AX, 
		item, 0, &err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_process_function_exit_wrapper,
		item, 0, &err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	
	/* Replace the original insn with 'ret'; 'dest_addr' and other 
	 * attributes of the node are preserved. */
	item = kedr_mk_ret(&ref_node->list, 1, &err);
	
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_mov_value32_to_ax((u32)ref_node->orig_addr, item, 0,
		&err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_warn_unreachable_wrapper,
		item, 0, &err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_ud2(item, 0, &err);
	
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
 * so that it is the same as the unbiased value of %base in the storage.
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
 * %base will contain the same value as on entry (the address of the primary 
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

/* ====================================================================== */
/* Transformation of the IR, phase 2 */
/* ====================================================================== */

/* TODO: describe, add asm snippet */
int
kedr_handle_end_of_normal_block(struct kedr_ir_node *ref_node, u8 base,
	u32 read_mask, u32 write_mask, u32 lock_mask)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_string("end of a block, ");
		debug_util_print_u64((u64)read_mask, 
			"read mask: 0x%08llx, ");
		debug_util_print_u64((u64)write_mask, 
			"write mask: 0x%08llx, ");
		debug_util_print_u64((u64)lock_mask, 
			"lock mask: 0x%08llx, ");
		debug_util_print_string("\n");
	}
	//<>
	
	// TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_jump_out_of_block(struct kedr_ir_node *ref_node, 	
	struct kedr_ir_node *end_node, u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_string("jump out of a block\n");
	}
	//<>
	
	// TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_setcc_cmovcc(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_u64((u64)num, "[#%llu] SETcc/CMOVcc\n");
	}
	//<>
	
	//TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_cmpxchg(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_u64((u64)num, "[#%llu] CMPXCHG\n");
	}
	//<>
	
	//TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_cmpxchg8b_16b(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_u64((u64)num, "[#%llu] CMPXCHG8B/16B\n");
	}
	//<>
	
	//TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_xlat(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_u64((u64)num, "[#%llu] XLAT\n");
	}
	//<>
	
	//TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_type_e_and_m(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_u64((u64)num, 
			"[#%llu] Generic type E or M\n");
	}
	//<>
	
	//TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_type_x(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_u64((u64)num, "[#%llu] Type X\n");
	}
	//<>
	
	//TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_type_y(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_u64((u64)num, "[#%llu] Type Y\n");
	}
	//<>
	
	//TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_type_xy(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	//<>
	if (strcmp(func_name, target_function) == 0) {
		debug_util_print_u64((u64)(ref_node->orig_addr - 
			(unsigned long)dbg_ifunc->addr), "0x%llx: ");
		debug_util_print_u64((u64)num, "[#%llu] Type XY\n");
	}
	//<>
	
	//TODO
	return 0;
}
/* ====================================================================== */
