/* ir_handlers.h - the functions to update the appropriate parts of the IR
 * during the instrumentation. */
#ifndef IR_HANDLERS_H_1609_INCLUDED
#define IR_HANDLERS_H_1609_INCLUDED

#include <linux/kernel.h> /* Basic types, etc. */

struct kedr_ir_node;
struct kedr_ifunc;
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
 *   'num'  - the number (index) of the current memory accessing instruction
 * in the block. */

/* ====================================================================== */
/* Transformation of the IR, phase 1 */
/* ====================================================================== */
int
kedr_handle_function_entry(struct list_head *ir, struct kedr_ifunc *func, 
	u8 base);

int
kedr_handle_function_exit(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_call_near_indirect(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_jmp_indirect_inner(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_jmp_indirect_out(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_pushad(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_popad(struct kedr_ir_node *ref_node, u8 base);

int
kedr_handle_general_case(struct kedr_ir_node *ref_node, u8 base);

/* ====================================================================== */
/* Transformation of the IR, phase 2 */
/* ====================================================================== */

/* '*_mask' - read, write and lock masks created when analyzing the 
 * instructions of this block. */
int
kedr_handle_end_of_normal_block(struct kedr_ir_node *end_node, u8 base,
	u32 read_mask, u32 write_mask, u32 lock_mask);

/* 'end_node' - the last reference node of the block that 'node' belongs to.
 */
int
kedr_handle_jump_out_of_block(struct kedr_ir_node *ref_node, 	
	struct kedr_ir_node *end_node, u8 base);

int
kedr_handle_setcc_cmovcc(struct kedr_ir_node *ref_node, u8 base, u8 num);

int
kedr_handle_cmpxchg(struct kedr_ir_node *ref_node, u8 base, u8 num);

int
kedr_handle_cmpxchg8b_16b(struct kedr_ir_node *ref_node, u8 base, u8 num);

int
kedr_handle_xlat(struct kedr_ir_node *ref_node, u8 base, u8 num);

int
kedr_handle_type_e_and_m(struct kedr_ir_node *ref_node, u8 base, u8 num);

int
kedr_handle_type_x(struct kedr_ir_node *ref_node, u8 base, u8 num);

int
kedr_handle_type_y(struct kedr_ir_node *ref_node, u8 base, u8 num);

int
kedr_handle_type_xy(struct kedr_ir_node *ref_node, u8 base, u8 num);
/* ====================================================================== */

#endif // IR_HANDLERS_H_1609_INCLUDED
