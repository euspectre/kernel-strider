/* ir_handlers.h - the functions to update the appropriate parts of the IR
 * during the instrumentation. */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>

#include "ifunc.h"
#include "primary_storage.h"
#include "ir.h"
#include "code_gen.h"
#include "internal_api.h"
#include "util.h"

extern int process_sp_accesses;

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

/* Find the size of the operand based on the attributes of the instruction 
 * and the given operand type. 
 * [NB] This function may not be generic, it does not cover all operand 
 * types. Still it should be enough for the instructions with addressing 
 * methods E, M, X and Y, which is OK for now. */
static unsigned int
get_operand_size_from_insn_attr(struct insn *insn, unsigned char opnd_type)
{
	BUG_ON(insn->length == 0);
	BUG_ON(insn->opnd_bytes == 0);
	
	switch (opnd_type)
	{
	case INAT_OPTYPE_B:
		/* Byte, regardless of operand-size attribute. */
		return 1;
	case INAT_OPTYPE_D:
		/* Doubleword, regardless of operand-size attribute. */
		return 4;
	case INAT_OPTYPE_Q:
		/* Quadword, regardless of operand-size attribute. */
		return 8;
	case INAT_OPTYPE_V:
		/* Word, doubleword or quadword (in 64-bit mode), depending 
		 * on operand-size attribute. */
		return insn->opnd_bytes;
	case INAT_OPTYPE_W:
		/* Word, regardless of operand-size attribute. */
		return 2;
	case INAT_OPTYPE_Z:
		/* Word for 16-bit operand-size or doubleword for 32 or 
		 * 64-bit operand-size. */
		return (insn->opnd_bytes == 2 ? 2 : 4);
	default: break;
	}
	return insn->opnd_bytes; /* just in case */
}

/* Determine the length of the memory area accessed by the given instruction
 * of type E or M. 
 * The instruction must be decoded before it is passed to this function. */
static unsigned int
get_mem_size_type_e_m(struct kedr_ir_node *node)
{
	insn_attr_t *attr = &node->insn.attr;
	struct insn *insn = &node->insn;
	
	BUG_ON(insn->length == 0);
	
	if (attr->addr_method1 == INAT_AMETHOD_E || 
	    attr->addr_method1 == INAT_AMETHOD_M) {
	    	return get_operand_size_from_insn_attr(insn, 
			attr->opnd_type1);
	}
	else if (attr->addr_method2 == INAT_AMETHOD_E || 
	    attr->addr_method2 == INAT_AMETHOD_M) {
	    	return get_operand_size_from_insn_attr(insn, 
			attr->opnd_type2);
	}

	/* The function must be called only for the instructions of
	 * type E or M. */
	BUG();
	return 0;
}

/* Determine the length of the memory area accessed by the given instruction
 * of type X, Y or XY at a time (i.e. if no REP prefix is present). 
 * For XY, only the first argument is checked because the other one
 * is the same size (see the description of MOVS and CMPS instructions).
 * 
 * The instruction must be decoded before it is passed to this function. */
static unsigned int
get_mem_size_type_x_y(struct kedr_ir_node *node)
{
	insn_attr_t *attr = &node->insn.attr;
	struct insn *insn = &node->insn;
	
	BUG_ON(insn->length == 0);
	
	if (attr->addr_method1 == INAT_AMETHOD_X || 
	    attr->addr_method1 == INAT_AMETHOD_Y) {
	    	return get_operand_size_from_insn_attr(insn, 
			attr->opnd_type1);
	}
	else if (attr->addr_method2 == INAT_AMETHOD_X || 
	    attr->addr_method2 == INAT_AMETHOD_Y) {
	    	return get_operand_size_from_insn_attr(insn, 
			attr->opnd_type2);
	}

	/* The function must be called only for the instructions of
	 * type X or Y. */
	BUG();
	return 0;
}

/* Get the offset of the field '_field' of the memory access record #_index 
 * from the beginning of the primary storage. */
#define KEDR_OFFSET_MEM_REC_FIELD(_index, _field) \
	(unsigned int)offsetof(struct kedr_primary_storage, mem_record) + \
	(unsigned int)((_index) * sizeof(struct kedr_mem_record)) + \
	(unsigned int)offsetof(struct kedr_mem_record, _field)

/* Check if the memory addressing expression uses %rsp/%esp. */
static int
expr_uses_sp(struct insn *insn)
{
	unsigned int expr_reg_mask = insn_reg_mask_for_expr(insn);
	return (expr_reg_mask & X86_REG_MASK(INAT_REG_CODE_SP));
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

/* Processing the end of a "normal" block (i.e. not a block for a single 
 * control transfer instruction) if the block has potential memory accesses.
 *
 * [NB] Jumps to 'block_end' are used to properly handle exits from the 
 * block (see "jump_out_of_block.data"). If 'dest_addr' field is nonzero in 
 * the storage, it is the actual destination address of such jump.
 *
 * 'temp' field is used as a scratch area. kedr_process_block_end() 
 * does not change it. But it zeroes 'dest_addr' to make sure it is 0 
 * when each block begins.
 * 
 * [NB] We make use of the fact that the wrapper functions preserve all the
 * registers except %rax. So we can be sure %rdx will not be changed by
 * kedr_process_block_end_wrapper().
 *
 * "Read mask", "write mask" and "lock mask" (LKMask) are determined by the 
 * instrumentation system when analyzing the block. Note that each of the 
 * masks is 32 bits in size both on x86-32 and on x86-64.
 *
 * "or" operation is used to accumulate read and write masks because some 
 * bits of these masks may have been already set in the block (cmpxchg, ...)
 * 
 * [NB] We must preserve the values of flags in this code. 'or' and 'test'
 * may change them, so pushf/popf are necessary.
 *
 * Code:
 * block_end:
 *	pushf
 *	or    <ReadMask32>, <offset_read_mask>(%base)
 *	or    <WriteMask32>, <offset_write_mask>(%base)
 *	mov   <LKMask32>, <offset_lock_mask>(%base)
 * 	mov   %rdx, <offset_dx>(%base)
 *	mov   <offset_dest_addr>(%base), %rdx
 * 	push  %rax
 * 	mov   %base, %rax
 * 	call  <kedr_process_block_end_wrapper>  # [NB] zeroes 'dest_addr'.
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
kedr_handle_end_of_normal_block(struct kedr_ir_node *end_node, u8 base,
	u32 read_mask, u32 write_mask, u32 lock_mask)
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
	
	/* 'val OR 0' is always 'val', no need to generate this OR insn */
	if (read_mask != 0) {
		item = kedr_mk_or_value32_to_slot(read_mask, base, 
			offsetof(struct kedr_primary_storage, read_mask), 
			item, 0, &err);
	}
	if (write_mask != 0) {
		item = kedr_mk_or_value32_to_slot(write_mask, base, 
			offsetof(struct kedr_primary_storage, write_mask),
			item, 0, &err);
	}
	
	/* After the block finishes, ps->lock_mask is still 0. No need to
	 * write 0 there. */
	if (lock_mask != 0) {
		item = kedr_mk_mov_value32_to_slot(lock_mask, base,
			offsetof(struct kedr_primary_storage, lock_mask),
			item, 0, &err);
	}
	item = kedr_mk_store_reg_to_spill_slot(INAT_REG_CODE_DX, base,
		item, 0, &err);
	item = kedr_mk_load_reg_from_ps(INAT_REG_CODE_DX, base, 
		(unsigned long)offsetof(struct kedr_primary_storage, 
			dest_addr),
		item, 0, &err);
	item = kedr_mk_push_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_mov_reg_to_reg(base, INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_call_rel32(
		(unsigned long)&kedr_process_block_end_wrapper, item, 0, 
		&err);
	item = kedr_mk_pop_reg(INAT_REG_CODE_AX, item, 0, &err);
	item = kedr_mk_store_reg_to_ps(INAT_REG_CODE_DX, base,
		(unsigned long)offsetof(struct kedr_primary_storage, temp),
		item, 0, &err);
	item = kedr_mk_test_reg_reg(INAT_REG_CODE_DX, item, 0, &err);
	item = kedr_mk_load_reg_from_spill_slot(INAT_REG_CODE_DX, base, 
		item, 0, &err);
	
	list_add(&node_jz->list, item);	
	item = kedr_mk_jcc(INAT_CC_Z, node, &node_jz->list, 1, &err);
	item = kedr_mk_popf(item, 0, &err);
	
	item = kedr_mk_jmp_offset_base(base, 
		(unsigned long)offsetof(struct kedr_primary_storage, temp),
		item, 0, &err);
	item = kedr_mk_popf(item, 0, &err);

	if (err == 0) {
		node->last = 
			list_entry(item, struct kedr_ir_node, list);
		node_jz->dest_inner = node->last; 
		/* 'jz' jumps to the last 'popf' */
	}
	else
		warn_fail(node);
	
	return err;
}

/* Handling of a direct jump (jmp near, jcc near) from a block to 
 * another block. We need to make sure a handler function for the block end
 * is called before the execution of another block begins.
 * [NB] The jumps using 8-bit displacements should have been replaced with 
 * jmp near or jcc near by this time, so we need to consider only these two
 * kinds of jumps.
 *
 * insn: <op> <disp32>
 * 
 * During the instrumentation, we know the destination node of the jump
 * ('node->dest_inner') but the exact address is not known until the end. 
 * Therefore, we need a kind of "relocation" that will replace the 
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
 * Code for jcc <disp32>:
 *	j<not cc> go_on
 *	mov <dest32>, <offset_dest_addr>(%base)	# same as above
 * 	jmp <disp_end> 				# same as above
 * go_on:
 *	# NB> <dest_addr> remains 0 in the storage if the jump is not taken.
 * 
 * <disp_end> is a displacement of the position just past the end of what
 * the last instruction of the block transforms to. A "block_end" handler
 * will be placed there that will dispatch the jump properly.
 * Set 'dest_inner' to the last node of the block for the insn above.
 * A flag should have been already set in the node to indicate it is a 
 * jump out ouf a block. So <disp_end> will be set properly during code
 * generation as the displacement of the node just after 'end_node->last'.
 */
int
kedr_handle_jump_out_of_block(struct kedr_ir_node *ref_node, 	
	struct kedr_ir_node *end_node, u8 base)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct kedr_ir_node *first = NULL;
	struct kedr_ir_node *node_jnotcc = NULL;
	struct kedr_ir_node *node_mov = NULL;
	u8 cc;
	
	BUG_ON(ref_node->jump_past_last == 0);
	
	/* First, create and add the node for 'mov'. */
	node_mov = kedr_ir_node_create();
	if (node_mov == NULL)
		return -ENOMEM;
	list_add(&node_mov->list, insert_after);
	kedr_mk_mov_value32_to_slot(0, base, 
		(u32)offsetof(struct kedr_primary_storage, dest_addr),
		&node_mov->list, 1, &err);
		
	/* Set 'dest_inner' for the node with 'mov' to be able to properly 
	 * relocate imm32 there later. */
	node_mov->dest_inner = ref_node->dest_inner;
	node_mov->needs_addr32_reloc = 1;
	first = node_mov;
	
	/* If it was a conditional jump originally, place 'j<not cc>' before
	 * 'mov'. */
	if (ref_node->insn.opcode.bytes[0] != 0xe9) {
		struct insn *insn = &ref_node->insn;
		
		/* It must be jcc near. */
		BUG_ON(insn->opcode.bytes[0] != 0x0f || 
			(insn->opcode.bytes[1] & 0xf0) != 0x80);
		
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
	
	/* Replace the original jump with a jump to the end of the block. */
	kedr_mk_jmp_to_inner(end_node, &ref_node->list, 1, &err);
	/* 'jump_past_last' remains 1, which is what we need. */
	ref_node->first = first;
	
	if (err != 0)
		warn_fail(ref_node);
	return err;
}

/* A helper function that generates the code given below to record the 
 * memory access from the instruction in the node 'node'. 
 * 
 * There are 2 cases: 
 * - %base is not used in the memory addressing expression <expr>; 
 * - %base is used there. 
 * %wreg is selected among the registers not used in <expr>, it should be
 * different from %base too.
 *
 * <orig_pc32> - the (lower 32 bits of) address of the original instruction 
 * (in the original kernel module).
 * <offset_recN_FFFF> - offset of field FFFF of the record #N in the 
 * storage.
 *
 * Code: 
 * 
 * Case 1: %base is not used in <expr>
 *	mov  %wreg, <offset_wreg>(%base)
 *	lea  <expr>, %wreg
 * The following part is the same in both cases:
 *	mov  <orig_pc32>, <offset_recN_pc>(%base)
 *	mov  <size>, <offset_recN_mem_size>(%base)
 *	mov  %wreg, <offset_recN_mem_addr>(%base)
 * 	mov  <offset_wreg>(%base), %wreg
 *
 * =======================================================================
 * Case 2: %base is used in <expr>. 
 *	mov  %wreg, <offset_wreg>(%base)
 * 	mov  %base, %wreg
 * 	mov  <offset_base>(%wreg), %base
 *	lea  <expr>, %base
 *	xchg %base, %wreg
 * The following part is the same in both cases:
 *	mov  <orig_pc32>, <offset_recN_pc>(%base)
 *	mov  <size>, <offset_recN_mem_size>(%base)
 *	mov  %wreg, <offset_recN_mem_addr>(%base)
 * 	mov  <offset_wreg>(%base), %wreg 
 * 
 * The function adds this code after 'item'. 
 * If 'size_hint' is 0, <size> is determined based on the instruction 
 * attributes, otherwise 'size_hint' is used as <size>. */
static struct list_head *
mk_record_access_common(struct kedr_ir_node *node, u8 base, u8 num, 
	unsigned int size_hint,
	struct list_head *item, int *err)
{
	u8 wreg;
	unsigned int expr_reg_mask;
	int base_is_used;
	unsigned int sz;
	
	if (*err != 0)
		return item;
	
	sz = (size_hint == 0 ? get_mem_size_type_e_m(node) : size_hint);
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
		item = kedr_mk_lea_expr_reg(&node->insn, base, item, 0, err);
		item = kedr_mk_xchg_reg_reg(base, wreg, item, 0, err);
	}
	else {
		item = kedr_mk_lea_expr_reg(&node->insn, wreg, item, 0, err);
	}
	
	item = kedr_mk_mov_value32_to_slot(
		(u32)(unsigned long)node->orig_addr, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, pc), item, 0, err);
	
	item = kedr_mk_store_reg_to_ps(wreg, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, addr), item, 0, err);

	item = kedr_mk_mov_value32_to_slot(
		sz, 
		base,
		KEDR_OFFSET_MEM_REC_FIELD(num, size), item, 0, err);
	
	item = kedr_mk_load_reg_from_spill_slot(wreg, base, item, 0, err);
	return item;
}

/* Processing memory accesses for the following instructions:
 * 	SETcc and CMOVcc
 *
 * Apply this before the instruction sequence.
 * 
 * Code:
 *	j<not cc> go_over
 * 	... # same as "E-general"
 * go_over:
 */
int
kedr_handle_setcc_cmovcc(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item;
	struct insn *insn = &ref_node->insn;
	struct kedr_ir_node *node_jcc;
	u8 cc;
	
	if (!process_sp_accesses && expr_uses_sp(&ref_node->insn))
		return 0;
	
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
	item = mk_record_access_common(ref_node, base, num, 0, item, &err);
	
	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
		node_jcc->last = list_entry(item, struct kedr_ir_node, list);
	}
	else
		warn_fail(ref_node);
	
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
 * In case of CMPXCHG, the size of the accessed area can be determined from
 * the instruction attributes. As for CMPXCHG8B/16B, it is 8 bytes if there
 * is no REX.W, 16 bytes otherwise (use 'size_hint').
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
handle_cmpxchg_impl(struct kedr_ir_node *ref_node, u8 base, u8 num, 
	unsigned int size_hint)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item = NULL;
	struct kedr_ir_node *node_jnz;
	
	if (!process_sp_accesses && expr_uses_sp(&ref_node->insn))
		return 0;
	
	/* Create the node for 'jnz' (to be filled later) */
	node_jnz = kedr_ir_node_create();
	if (node_jnz == NULL)
		return -ENOMEM;
	
	mk_record_access_common(ref_node, base, num, size_hint, 
		insert_after, &err);
	list_add(&node_jnz->list, &ref_node->last->list);
	
	/* Make sure the jump will lead to the node following the 
	 * instruction sequence. */
	item = kedr_mk_jcc(INAT_CC_NZ, ref_node, &node_jnz->list, 1, &err);
	node_jnz->jump_past_last = 1;
	
	item = kedr_mk_pushf(item, 0, &err);
	item = kedr_mk_or_value32_to_slot(((u32)1 << num), base, 
		offsetof(struct kedr_primary_storage, write_mask),
		item, 0, &err);
	item = kedr_mk_popf(item, 0, &err);
	
	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
		ref_node->last = list_entry(item, struct kedr_ir_node, list);
	}
	else
		warn_fail(ref_node);
	
	return err;
}

int
kedr_handle_cmpxchg(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	return handle_cmpxchg_impl(ref_node, base, num, 0);
}

int
kedr_handle_cmpxchg8b_16b(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	u8 rex = ref_node->insn.rex_prefix.bytes[0]; /* 0 on x86-32 */
	return handle_cmpxchg_impl(ref_node, base, num, 
		(X86_REX_W(rex) ? 16 : 8));
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
 *	mov  <orig_pc32>, <offset_recN_pc>(%base)
 *	mov  1, <offset_recN_mem_size>(%base)
 *	mov  %rax, <offset_recN_mem_addr>(%base)
 * 	mov  <offset_ax>(%base), %rax	# restore %rax
 */
int
kedr_handle_xlat(struct kedr_ir_node *ref_node, u8 base, u8 num)
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
	
	item = kedr_mk_mov_value32_to_slot(
		(u32)(unsigned long)ref_node->orig_addr, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, pc), item, 0, &err);
	
	item = kedr_mk_store_reg_to_ps(INAT_REG_CODE_AX, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, addr), item, 0, &err);

	item = kedr_mk_mov_value32_to_slot(
		1, 
		base,
		KEDR_OFFSET_MEM_REC_FIELD(num, size), item, 0, &err);
	
	item = kedr_mk_load_reg_from_spill_slot(INAT_REG_CODE_AX, base,
		item, 0, &err);

	if (err == 0)
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
	
	return err;
}

/* See mk_record_access_common(). */
int
kedr_handle_type_e_and_m(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	
	if (!process_sp_accesses && expr_uses_sp(&ref_node->insn))
		return 0;
	
	mk_record_access_common(ref_node, base, num, 0, insert_after, &err);
	
	if (err == 0)
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
	else
		warn_fail(ref_node);
	
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
 *	add  <S>, %wreg		# <S> is 1, 2, 4, or 8 => 8 bits is enough
 * 	neg  %treg		# treg = -treg; now treg == length
 *
 * record_access:
 *
 * No matter in which direction the data processing was made, %wreg is now 
 * the start address of the accessed memory block and %treg is the length of
 * the block in bytes. [%wreg, %wreg + %treg)
 *	mov  <orig_pc32>, <offset_recN_pc>(%base)
 *	mov  %wreg, <offset_recN_mem_addr>(%base)
 *	mov  %treg, <offset_recN_mem_size>(%base)
 * out:
 *	popf
 *	mov  <offset_treg>(%base), %treg
 *	mov  <offset_wreg>(%base), %wreg 
 */
static int 
handle_type_x_and_y_impl(struct kedr_ir_node *ref_node, u8 base, u8 num, 
	u8 amethod)
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
		
	unsigned int sz = get_mem_size_type_x_y(ref_node);
	
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
	item = kedr_mk_add_value8_to_reg((u8)sz, wreg, item, 0, &err);
	item = kedr_mk_neg_reg(treg, item, 0, &err);
	
	kedr_mk_mov_value32_to_slot(
		(u32)(unsigned long)ref_node->orig_addr, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, pc), 
		&node_record_access->list, 1, &err);
	list_add(&node_record_access->list, item);
	item = &node_record_access->list;
		
	item = kedr_mk_store_reg_to_ps(wreg, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, addr), item, 0, &err);
	item = kedr_mk_store_reg_to_ps(treg, base,
		KEDR_OFFSET_MEM_REC_FIELD(num, size), item, 0, &err);
	
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
	else
		warn_fail(ref_node);
	
	return err;
}

int
kedr_handle_type_x(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	return handle_type_x_and_y_impl(ref_node, base, num, INAT_AMETHOD_X);
}

int
kedr_handle_type_y(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	return handle_type_x_and_y_impl(ref_node, base, num, INAT_AMETHOD_Y);
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
 *	mov  <orig_pc32>, <offset_recN_pc>(%base)
 *	mov  %rax, <offset_recN_mem_addr>(%base)
 *	mov  %rcx, <offset_recN_mem_size>(%base)
 *	mov  <orig_pc32>, <offset_recN+1_pc>(%base)
 *	mov  %rdx, <offset_recN+1_mem_addr>(%base)
 *	mov  %rcx, <offset_recN+1_mem_size>(%base)
 * out:
 *	popfq
 *	mov  <offset_cx>(%base), %rcx
 *	mov  <offset_dx>(%base), %rdx
 *	mov  <offset_ax>(%base), %rax
 */
int
kedr_handle_type_xy(struct kedr_ir_node *ref_node, u8 base, u8 num)
{
	int err = 0;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item = NULL;
	struct kedr_ir_node *node_record_access = NULL;
	struct kedr_ir_node *node_out = NULL;
	
	unsigned int sz = get_mem_size_type_x_y(ref_node);
	
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
	kedr_mk_mov_value32_to_slot(
		(u32)(unsigned long)ref_node->orig_addr, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, pc), 
		&node_record_access->list, 1, &err);
	list_add(&node_record_access->list, item);
	item = &node_record_access->list;
	
	item = kedr_mk_store_reg_to_ps(INAT_REG_CODE_AX, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, addr), item, 0, &err);
	item = kedr_mk_store_reg_to_ps(INAT_REG_CODE_CX, base,
		KEDR_OFFSET_MEM_REC_FIELD(num, size), item, 0, &err);
	
	++num;
	item = kedr_mk_mov_value32_to_slot(
		(u32)(unsigned long)ref_node->orig_addr, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, pc), item, 0, &err);
	item = kedr_mk_store_reg_to_ps(INAT_REG_CODE_DX, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, addr), item, 0, &err);
	item = kedr_mk_store_reg_to_ps(INAT_REG_CODE_CX, base,
		KEDR_OFFSET_MEM_REC_FIELD(num, size), item, 0, &err);

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
	else
		warn_fail(ref_node);
	
	return err;
}

/* Processing memory accesses for direct memory offset MOVs (opcodes A0-A3).
 * 
 * Apply this before the instruction sequence.
 * Code:
 * 	mov  <orig_pc32>, <offset_recN_pc>(%base)
 *	mov  <size>, <offset_recN_mem_size>(%base)
 *	mov  <addr32>, <offset_recN_mem_addr>(%base)
 *
 * <addr32> - the lower 32 bits of the address of the memory location to be
 * accessed. 
 * [NB] On x86-64, <addr32> is sign-extended to 64 bits when written to
 * the primary storage. This introduces a possibility to confuse two 
 * addresses having the same lower 32 bits. This can not happen when the 
 * target module accesses its own data, the data of another module or of the 
 * kernel proper: according to Documentation/x86/x86_64/mm.txt, the kernel
 * proper is mapped within the region [ffffffff80000000, ffffffffa0000000), 
 * the modules - within [ffffffffa0000000, fffffffffff00000). The confusion
 * may occur only if a module uses direct offset MOVs to access both 
 * some data inside of [ffffffff80000000, fffffffffff00000) and some data
 * outside of that region and the lower 32 bits of the addresses are the 
 * same. This seems to be unlikely. In addition, direct offset MOVs do not
 * seem to be used very often in the modules on x86-64, so let us neglect 
 * that unlikely deficiency.
 */
int
kedr_handle_direct_offset_mov(struct kedr_ir_node *ref_node, u8 base, 
	u8 num)
{
	int err = 0;
	struct insn *insn = &ref_node->insn;
	struct list_head *insert_after = ref_node->first->list.prev;
	struct list_head *item;
	unsigned char opnd_type;
	
	/* The address is the argument of with addressing method "O". 
	 * There must always be such argument. */
	if (insn->attr.addr_method1 == INAT_AMETHOD_O)
		opnd_type = insn->attr.opnd_type1;
	else if (insn->attr.addr_method2 == INAT_AMETHOD_O)
		opnd_type = insn->attr.opnd_type2;
	else 
		BUG();
	
	item = kedr_mk_mov_value32_to_slot(
		(u32)(unsigned long)ref_node->orig_addr, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, pc), insert_after, 0, &err);
	
	item = kedr_mk_mov_value32_to_slot((u32)insn->moffset1.value, base, 
		KEDR_OFFSET_MEM_REC_FIELD(num, addr), item, 0, &err);

	item = kedr_mk_mov_value32_to_slot(
		(u32)get_operand_size_from_insn_attr(insn, opnd_type), base,
		KEDR_OFFSET_MEM_REC_FIELD(num, size), item, 0, &err);

	if (err == 0) {
		ref_node->first = list_entry(insert_after->next, 
			struct kedr_ir_node, list);
	}
	else
		warn_fail(ref_node);
	
	return err;
}
/* ====================================================================== */
