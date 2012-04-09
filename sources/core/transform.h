/* transform.h - the functions to transform the appropriate parts of 
 * the IR during the instrumentation. */
#ifndef TRANSFORM_H_1609_INCLUDED
#define TRANSFORM_H_1609_INCLUDED

#include <linux/kernel.h> /* Basic types, etc. */

struct kedr_ir_node;
struct kedr_ifunc;
struct kedr_block_info;
struct list_head;
/* ====================================================================== */

/* All kedr_handle_*() functions return 0 on success or a negative error 
 * code in case of failure. 
 * 
 * The commonly used parameters are: 
 *   'node' - the reference IR node. The handler function may insert new 
 * nodes before and/or after it as needed (to be exact, before 
 * 'ref_node->first' and after 'ref_node->last'). The function must adjust 
 * 'ref_node->first' and 'ref_node->last' appropriately. 
 * Note that if a handler fails, these 'first' and 'last' fields of the node
 * may be in an unspecified state. Do not use them in this case.
 *   'base' - the code of the base register chosen for the function. 
 *   'func' - the struct kedr_ifunc instance corresponding to the function
 * to be instrumented. 
 *   'num' - the number of the tracked memory operation in the block, 
 * starting from 0.
 *   'nval' - the number of the first slot in local_storage::values[] that
 * the instruction can use. Some instruction (e.g. string operations) may
 * need more than one slot. */

/* ====================================================================== */
/* Transformation of the IR, phase 1 */
/* ====================================================================== */
int
kedr_handle_function_entry(struct list_head *ir, struct kedr_ifunc *func, 
	u8 base);

int
kedr_handle_function_exit(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_call_indirect(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_jmp_indirect_inner(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_jmp_indirect_out(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_call_rel32_out(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_jxx_rel32_out(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_pushad(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_popad(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_general_case(struct kedr_ir_node *ref_node, u8 base);

/* ====================================================================== */
/* Transformation of the IR, phase 2 */
/* ====================================================================== */
int 
kedr_handle_block_end_no_jumps(struct kedr_ir_node *start_node, 
	struct kedr_ir_node *end_node, u8 base);

int 
kedr_handle_block_end(struct kedr_ir_node *start_node, 
	struct kedr_ir_node *end_node, u8 base);

int 
kedr_handle_locked_op(struct kedr_ir_node *ref_node, u8 base);

int 
kedr_handle_io_mem_op(struct kedr_ir_node *ref_node, u8 base);

int 
kedr_handle_barrier_other(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_jump_out_of_block(struct kedr_ir_node *ref_node, 	
	struct kedr_ir_node *end_node, u8 base);

int
kedr_handle_setcc_cmovcc(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval);

int
kedr_handle_cmpxchg(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval);

int
kedr_handle_cmpxchg8b_16b(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval);

int
kedr_handle_xlat(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval);

int
kedr_handle_type_e_and_m(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval);

int
kedr_handle_direct_offset_mov(struct kedr_ir_node *ref_node, u8 base, 
	unsigned int num, unsigned int nval);

int
kedr_handle_type_x(struct kedr_ir_node *ref_node, 
	struct kedr_block_info *info, u8 base, 
	unsigned int num, unsigned int nval);

int
kedr_handle_type_y(struct kedr_ir_node *ref_node, 
	struct kedr_block_info *info, u8 base, 
	unsigned int num, unsigned int nval);

int
kedr_handle_type_xy(struct kedr_ir_node *ref_node, 
	struct kedr_block_info *info, u8 base, 
	unsigned int num, unsigned int nval);
/* ====================================================================== */

#endif // TRANSFORM_H_1609_INCLUDED
