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

//<> For debugging only
#include "debug_util.h"
extern char *target_function;
extern const char *func_name;
//<>
/* ====================================================================== */

// !!!!!!!! TODO: use node->first and node->next when adding stuff !!!!!!!!
// And do not forget to update these fields after that.

/* ir - the IR of the function.
 * TODO: describe, add asm snippet */
int
kedr_handle_function_entry(struct list_head *ir, struct kedr_ifunc *func, 
	u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("entry\n");
	//<>

	// TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_function_exit(struct kedr_ir_node *node, u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("exit\n");
	//<>
	
	// TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_end_of_normal_block(struct kedr_ir_node *node, u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("end of a block\n");
	//<>
	
	// TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_jump_out_of_block(struct kedr_ir_node *node, u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("jump out of a block\n");
	//<>
	
	// TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_call_near_indirect(struct kedr_ir_node *node, u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("call near indirect\n");
	//<>
	
	// TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_jump_near_indirect(struct kedr_ir_node *node, u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("jump near indirect\n");
	//<>
	
	// TODO
	return 0;
}

#ifdef CONFIG_X86_64
/* Neither PUSHAD nor POPAD instructions are available on x86-64, so if 
 * these handlers are called there, it is our bug. */
int
kedr_handle_pushad(struct kedr_ir_node *node, u8 base)
{
	BUG();
	return 0;
}

int
kedr_handle_popad(struct kedr_ir_node *node, u8 base)
{
	BUG();
	return 0;
}

#else /* x86-32 */
/* TODO: describe, add asm snippet */
int
kedr_handle_pushad(struct kedr_ir_node *node, u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("pushad\n");
	//<>
	
	// TODO
	return 0;
}

/* TODO: describe, add asm snippet */
int
kedr_handle_popad(struct kedr_ir_node *node, u8 base)
{
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("popad\n");
	//<>
	
	//TODO
	return 0;
}
#endif

/* TODO: describe, add asm snippet */
int
kedr_handle_general_case(struct kedr_ir_node *node, u8 base)
{
	if (insn_is_noop(&node->insn)) {
		//<>
		if (strcmp(func_name, target_function) == 0)
			debug_util_print_string("no-op\n");
		//<>
		return 0;
	}
	
	//<>
	if (strcmp(func_name, target_function) == 0)
		debug_util_print_string("general\n");
	//<>
	
	// TODO
	return 0;
}
/* ====================================================================== */
