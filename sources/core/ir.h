/* ir.h - definition of the intermediate representation (IR) of the code. */

#ifndef IR_H_1801_INCLUDED
#define IR_H_1801_INCLUDED

#include <linux/kernel.h>
#include <linux/list.h>

#include <kedr/asm/insn.h> /* instruction analysis facilities */
#include <kedr/object_types.h>

struct kedr_block_info;
struct kedr_call_info;
struct kedr_ifunc;
struct kedr_i13n;
/* ====================================================================== */

/* A block of code in a function contains one or more machine instructions. 
 * 
 * The rules used to split the code of a function into such blocks: 
 * - if an instruction may transter control outside of the current function,
 *    it constitutes a separate block; note that in addition to some of the 
 *    calls and jumps, the instructions like 'ret' and 'int' fall into this 
 *    group;
 * - if an instruction transfers control to a location before it within the 
 *    function (a "backward jump" in case of 'for'/'while'/'do' constructs, 
 *    etc.), it constitutes a separate block;
 *    note that rep-prefixed instructions do not fall into this group;
 * - each 'jmp near r/m32' instruction constitutes a separate block, same
 *    for 'jmp near r/m64';
 * - if the destination of a near indirect jump is inside of the function,
 *    it must be a beginning of some block;
 * - if an instruction transfers control to a location before it within the 
 *    function, it is allowed to transfer control only to the beginning of 
 *    a block; 
 * - it is allowed for a block to contain the instructions that transfer 
 *    control forward within the function, not necessarily within the block
 *    such instructions need not be placed in separate blocks;
 * - the number of the instructions accessing memory is limited: the number
 *    of data items needed in local storage for these instructions should
 *    not exceed KEDR_MAX_LOCAL_VALUES;
 * - if an instruction is a locked update (an instruction with LOCK prefix 
 *    or XCHG accessing memory), it constitutes a separate block;
 * - if an instruction is an I/O operation, it constitutes a separate block;
 * - if an instruction is a memory barrier but is not a locked update or I/O
 *    operation (*fence, cpuid, ...), it constitutes a separate block;
 * 
 * Note that the destinations of forward jumps do not need to be at the 
 * beginning of a block. Jumps into a block are allowed (so are the jumps
 * out of a block). */
/* ====================================================================== */

/* Types of the code blocks. The blocks of different types may need 
 * different end handlers, etc.
 *
 * [NB] A "common" block is a block that does not contain backward jumps, 
 * indirect jumps, control transfers outside of the function, locked 
 * operations, I/O operations, other kinds of memory barriers. */
enum kedr_cb_type
{
	/* This type is assigned to the nodes that are not the first in 
	 * their block. */
	KEDR_CB_NONE = 0,
	
	/* A block for a call/jump near relative that transfers control 
	 * backwards within the function. */
	KEDR_CB_JUMP_BACKWARDS,
	
	/* A common block without memory operations to be tracked. */
	KEDR_CB_COMMON_NO_MEM_OPS,
	
	/* A common block with memory operations to be tracked. See also 
	 * 'block_has_jumps_out' flag in the starting node of the block to
	 * determine how to handle the end of the block. */
	KEDR_CB_COMMON,
	
	/* A block with a locked update operation. */
	KEDR_CB_LOCKED_UPDATE,
	
	/* A block with an I/O operation accessing memory. */
	KEDR_CB_IO_MEM_OP,
	
	/* A block for other kinds of memory barriers (i.e. not the locked
	 * updates or I/O operations accessing memory). */
	KEDR_CB_BARRIER_OTHER,
	
	/* Call near relative that leads outside of the function. 
	 * [NB] This does not apply to the tricks like 'call $0x5', those 
	 * are considered ordinary instructions. */
	KEDR_CB_CALL_REL32_OUT,
	
	/* Jump (JMP, Jcc) near relative that leads outside of the 
	 * function. */
	KEDR_CB_JUMP_REL32_OUT,
	
	/* call <expr> (call near indirect). Such calls are assumed to lead
	 * outside of the function. */
	KEDR_CB_CALL_INDIRECT,
	
	/* jmp <expr> (jmp near indirect) that uses a jump table and 
	 * transfers control within the function. */
	KEDR_CB_JUMP_INDIRECT_INNER,
	
	/* jmp <expr> (jmp near indirect) that leads outside of the 
	 * function. */
	KEDR_CB_JUMP_INDIRECT_OUT,
	
	/* An instruction of some other kind that transfers the control 
	 * outside of the function (CALL/JMP far, INT, RET, UD2, ...). */
	KEDR_CB_CONTROL_OUT_OTHER
};

/* A node of the IR. There are two main kinds of the nodes:
 * - "reference nodes" are the nodes that corresponded to the instructions
 *     in the original code when the nodes were created (the contents of 
 *     the nodes may have changed since then), such nodes have nonzero 
 *     'orig_addr';
 * - all other nodes that have been created during instrumentation, such 
 *     nodes have 0 as 'orig_addr'. */
struct kedr_ir_node;
struct kedr_ir_node
{
	/* The ordered list of the instructions. */
	struct list_head list; 
	
	/* This field allows to place the node into a hash table when it is
	 * needed. */
	struct hlist_node hlist;
	
	/* A buffer containing the instruction. */
	u8 insn_buffer[X86_MAX_INSN_SIZE];
	
	/* The instruction decoded from insn_buffer */
	struct insn insn;
	
	/* Address of the instruction in the original function, 0 if the 
	 * instruction was added only during the instrumentation. */
	unsigned long orig_addr;
	
	/* Offset of the instruction in the instrumented instance of the 
	 * function from the beginning of that instance. */
	long offset;
	
	/* During the instrumentation, the instruction may be replaced with
	 * a sequence of instructions. 'first' points to the first node
	 * of that sequence, 'last' - to the last one. If no instructions 
	 * have been added, both 'first' and 'last' point to this very
	 * node. */
	struct kedr_ir_node *first;
	struct kedr_ir_node *last;
	
	/* If the node represents a direct relative jump within the
	 * current function, 'dest_inner' points to the node corresponding
	 * to the destination of the jump. This field is NULL if the node
	 * represents something else (this can also be used when choosing 
	 * whether to use a short or a near jump). */
	struct kedr_ir_node *dest_inner;
	
	/* (see insn_jumps_to()) */
	unsigned long dest_addr;
	
	/* If the node represents a call/jmp rel32 that refers to something
	 * outside of the original function or represents an instruction 
	 * with RIP-relative addressing mode, 'iprel_addr' is the address it 
	 * refers to. The address should be the same in the instrumented 
	 * code but the offset will change. 
	 * 
	 * This field remains 0 if the node represents something else. 
	 * 
	 * [NB] Although 'dest_addr' is available, 'iprel_addr' is necessary
	 * too. For example, the former is 0 for the instructions with 
	 * IP-relative addressing and is generally used to process control 
	 * transfer instructions when spliting the code into blocks. 
	 * The latter is mainly used to prepare relocation of the 
	 * instrumented code. Among other things, 'iprel_addr' is 0 for the
	 * control transfer instructions without IP-relative addressing 
	 * (e.g. 'ret', 'int'). */
	unsigned long iprel_addr; 
	
	/* KEDR_CB_NONE if this node is not a starting node of a block, type
	 * of the block otherwise. */
	enum kedr_cb_type cb_type;
	
	/* This is the pointer to the last reference node of the block.
	 * Meaningful only for the starting nodes of KEDR_CB_COMMON blocks,
	 * should remain NULL for other nodes. */
	struct kedr_ir_node *end_node;
	
	/* If this is the starting node of a block (block_starts != 0) and
	 * the block has 'kedr_block_info' instance associated with it, here
	 * is the pointer to that instance. In all other cases 'block_info'
	 * will be NULL. 
	 * Note that the 'kedr_block_info' instance is owned by the function
	 * object (kedr_ifunc) rather than by this node. The owner will take
	 * care of deleting that instance when it is appropriate. */
	struct kedr_block_info *block_info;
	
	/* Meaningful only for a node that represents a function call
	 * (call/jmp out, direct or indirect). Each call has 
	 * 'kedr_call_info' instance associated with it.
	 * Note that the 'kedr_call_info' instance is owned by the function
	 * object (kedr_ifunc) rather than by this node. The owner will take
	 * care of deleting that instance when it is appropriate. */
	struct kedr_call_info *call_info;
	
	/* Register usage mask for the instruction. To simplify debugging,
	 * its default value should be as if the instruction used all the 
	 * general-purpose registers. */
	unsigned int reg_mask;
	
	/* Meaningful only if the instruction is a memory barrier. */
	enum kedr_barrier_type barrier_type;
		
	/* Nonzero if the node represents a jump which destination is not 
	 * 'dest_inner->first' as for many other nodes but rather 
	 * 'dest_inner->last->(next)'. Among reference nodes, this is the 
	 * case only for the forward jumps out of a common block.
	 * This flag can also be set for some nodes added during the 
	 * instrumentation if they represent jumps with destination being
	 * 'dest_inner->last->(next)'. Default value: 0. */
	unsigned int jump_past_last : 1;
	
	/* Nonzero if this IR node corresponds to a start of a code block
	 * in the original code, 0 otherwise. Default value: 0. */
	unsigned int block_starts : 1;
	
	/* Nonzero if the node corresponds to an inner jmp near indirect
	 * that uses a jump table. Default value: 0. */
	unsigned int inner_jmp_indirect : 1;
	
	/* Nonzero if a relocation of type KEDR_RELOC_ADDR32 should be 
	 * performed for the instruction. This is used in handling of the
	 * forward jumps out of the blocks. Default value: 0. */
	unsigned int needs_addr32_reloc : 1;
	
	/* Meaningful only for a starting node of a common block. Nonzero
	 * if there is a forward jump in the block that leads outside of the
	 * block but still somewhere within the function. 
	 * Default value: 0. */
	unsigned int block_has_jumps_out : 1;
	
	/* Nonzero if the node corresponds to a memory operation that 
	 * should be tracked (i.e. an operation for which a memory event 
	 * should be reported). */
	unsigned int is_tracked_mem_op : 1;
	
	/* Nonzero if the node corresponds to a string operation. */
	unsigned int is_string_op : 1;
	
	/* Nonzero if the node corresponds to a string operation of type XY
	 * (MOVS, CMPS). Meaningful only for the nodes with 
	 * is_string_op != 0. For other nodes, it should be 0. */
	unsigned int is_string_op_xy : 1;
};

/* Creates the IR for the given function and prepares some other facilities
 * needed for the instrumented code. Among other things:
 * - the short jumps are replaced with the equivalent near jumps;
 * - the nodes for the jumps are properly linked to the nodes for their
 * destinations;
 * - the jump tables for the instrumented instance (if it has any) are 
 * created and filled with the pointers to the corresponding IR nodes at
 * this stage (to be replaced later by the actual addresses);
 * - the IR is split into blocks, the first node of each block is marked
 * as such. 
 * - kedr_block_info instances are created for the blocks where it is 
 * needed and added to the list of such structures in 'func'. 
 * 
 * The list head pointed to by 'ir' should be intialized before this 
 * function is called. */
int 
kedr_ir_create(struct kedr_ifunc *func, struct kedr_i13n *i13n, 
	struct list_head *ir);

/* Transforms the IR for the given function to instrument it. */
int 
kedr_ir_instrument(struct kedr_ifunc *func, struct list_head *ir);

/* Prepares the instrumented instance of the function from the IR in the 
 * temporary memory buffer. The resulting code will only need relocation
 * before it can be used. 
 * On success, 'tbuf' and 'i_size' become defined, 'tbuf' pointing to that
 * buffer. In case of failure, 'tbuf' will remain NULL.
 * 
 * [NB] The value of 'i_addr' will be defined at the deployment stage, when 
 * the function is copied to its final location. 
 *
 * [NB] Here, we can assume that the size of the function is not less than
 * the size of 'jmp near rel32'.
 *
 * The function allocates memory for the instrumented instance as needed.
 * 
 * The instructions to be relocated at the deployment phase (call/jmp rel32 
 * outside of the function, instructions with RIP-relative addressing) will 
 * be listed in 'func->relocs'.
 * 
 * Among other things, the function fills the jump tables (if any) for the 
 * instrumented instance with the offsets of the appropriate positions in 
 * the instrumented function. The offsets should be replaced with the 
 * complete addresses during the deployment phase.
 * 
 * The return value is 0 on success and a negative error code on failure. */
int 
kedr_ir_generate_code(struct kedr_ifunc *func, struct list_head *ir);

/* Use this function to destroy the IR when it is no longer needed (i.e. 
 * after the instrumented instance of the function has been prepared). 
 * The head itself ('ir') is not freed by this function. 
 * The function object (struct kedr_ifunc) the IR was created for is not 
 * affected. 
 *
 * The function does nothing if the IR is empty (i.e. there are no nodes
 * except the head). */
void
kedr_ir_destroy(struct list_head *ir);

/* Constructs an IR node with all fields initialized to their default 
 * values.
 * The function returns the pointer to the constructed and initialized node
 * on success, NULL if there is not enough memory to complete the operation.
 */
struct kedr_ir_node *
kedr_ir_node_create(void);

/* Destroys the node and release memory it occupies. 
 * If 'node' is NULL, the function does nothing. */
void
kedr_ir_node_destroy(struct kedr_ir_node *node);


#endif // IR_H_1801_INCLUDED
