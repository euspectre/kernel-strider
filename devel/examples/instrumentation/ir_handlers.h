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
 * nodes before and/or after it as needed (to be exact, before 'node->first'
 * and after 'node->last'). The function must adjust 'node->first' and 
 * 'node->last' appropriately. 
 *   'base' - the code of the base register chosen for the function. 
 *   'func' - the struct kedr_ifunc instance corresponding to the function
 * to be instrumented. */

int
kedr_handle_function_entry(struct list_head *ir, struct kedr_ifunc *func, 
	u8 base);

int
kedr_handle_function_exit(struct kedr_ir_node *node, u8 base);

int
kedr_handle_end_of_normal_block(struct kedr_ir_node *node, u8 base);

int
kedr_handle_jump_out_of_block(struct kedr_ir_node *node, u8 base);

int
kedr_handle_call_near_indirect(struct kedr_ir_node *node, u8 base);

int
kedr_handle_jump_near_indirect(struct kedr_ir_node *node, u8 base);

int
kedr_handle_pushad(struct kedr_ir_node *node, u8 base);

int
kedr_handle_popad(struct kedr_ir_node *node, u8 base);

int
kedr_handle_general_case(struct kedr_ir_node *node, u8 base);

// TODO
/* ====================================================================== */

#endif // IR_HANDLERS_H_1609_INCLUDED
