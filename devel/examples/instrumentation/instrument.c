/* instrument.c - instrumentation-related facilities. */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/hash.h>

#include <kedr/asm/insn.h> /* instruction analysis support */

#include "instrument.h"
#include "util.h"
#include "detour_buffer.h"
#include "primary_storage.h"
#include "ir.h"
#include "code_gen.h"
#include "operations.h"

//<> For debugging only
#include "debug_util.h"
extern char *target_function;
//<>

/* ====================================================================== */

/* [NB] A block of code in a function contains one or more machine
 * instructions. 
 * The rules used to split the function code into such blocks: 
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
 * - near indirect jumps must always transfer control to the beginning of
 *    a block;
 * - if an instruction transfers control to a location before it within the 
 *    function, it is allowed to transfer control only to the beginning of 
 *    a block; 
 * - it is allowed for a block to contain the instructions that transfer 
 *    control forward within the function, not necessarily within the block
 *    such instructions need not be placed in separate blocks;
 * - a block may contain no more than KEDR_MEM_NUM_RECORDS instructions 
 *    accessing memory.
 * 
 * Note that the destinations of forward jumps do not need to be at the 
 * beginning of a block. Jumps into a block are allowed (so are the jumps
 * out of a block). */
/* ====================================================================== */

/* Parameters of a hash map to be used to lookup IR nodes by the addresses 
 * of the corresponding machine instructions in the original code.
 * 
 * KEDR_IF_TABLE_SIZE - number of buckets in the table. */
#define KEDR_IF_HASH_BITS   10
#define KEDR_IF_TABLE_SIZE  (1 << KEDR_IF_HASH_BITS)

/* The hash map (original address; IR node). */
static struct hlist_head node_map[KEDR_IF_TABLE_SIZE];

/* Initialize the hash map of nodes. */
static void
node_map_init(void)
{
	unsigned int i = 0;
	for (; i < KEDR_IF_TABLE_SIZE; ++i)
		INIT_HLIST_HEAD(&node_map[i]);
}

/* Remove all the items from the node map. */
static void
node_map_clear(void)
{
	struct hlist_node *pos = NULL;
	struct hlist_node *tmp = NULL;
	unsigned int i = 0;
	
	for (; i < KEDR_IF_TABLE_SIZE; ++i) {
		hlist_for_each_safe(pos, tmp, &node_map[i])
			hlist_del(pos);
	}
}

/* Add a given node to the hash map with the address of the corresponding 
 * instruction in the original function ('node->orig_addr') as a key. */
static void
node_map_add(struct kedr_ir_node *node)
{
	unsigned long bucket; 
	INIT_HLIST_NODE(&node->hlist);
	
	bucket = hash_ptr((void *)(node->orig_addr), KEDR_IF_HASH_BITS);
	hlist_add_head(&node->hlist, &node_map[bucket]);
}

/* Find the IR node corresponding to the instruction at the given address
 * in the original function. 
 * Returns the pointer to the node if found, NULL otherwise. */
static struct kedr_ir_node *
node_map_lookup(unsigned long orig_addr)
{
	struct kedr_ir_node *node = NULL;
	struct hlist_node *tmp = NULL;
	
	unsigned long bucket = 
		hash_ptr((void *)(orig_addr), KEDR_IF_HASH_BITS);
	hlist_for_each_entry(node, tmp, &node_map[bucket], hlist) {
		if (node->orig_addr == orig_addr)
			return node;
	}
	return NULL; 
}

/* ====================================================================== */

/* Nonzero if 'addr' is the address of some location in the "init" area of 
 * the module (may be code or data), 0 otherwise. */
static int
is_init_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);
	if ((mod->module_init != NULL) &&
	    (addr >= (unsigned long)(mod->module_init)) &&
	    (addr < (unsigned long)(mod->module_init) + mod->init_size))
		return 1;
	
	return 0;
}

/* Nonzero if 'addr' is the address of some location in the "core" area of 
 * the module (may be code or data), 0 otherwise. */
static int
is_core_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);

	if ((mod->module_core != NULL) &&
	    (addr >= (unsigned long)(mod->module_core)) &&
	    (addr < (unsigned long)(mod->module_core) + mod->core_size))
		return 1;
	
	return 0;
}
/* ====================================================================== */

/* Similar to insn_reg_mask() but also takes function calls into
 * account. If 'insn' transfers control outside of the function
 * 'func', the register_usage_mask() considers all the scratch general
 * purpose registers used and updates the mask accordingly. 
 * 
 * It is possible that the instruction does not actually use this many
 * registers. For now, we take a safer, simpler but less optimal route 
 * in such cases. */
static unsigned int 
register_usage_mask(struct insn *insn, struct kedr_ifunc *func)
{
	unsigned int reg_mask;
	unsigned long dest;
	unsigned long start_addr = (unsigned long)func->addr;
	u8 opcode;
	
	BUG_ON(insn == NULL);
	BUG_ON(func == NULL);
	
	/* Decode at least the opcode because we need to handle some 
	 * instructions separately ('ret' group). */
	insn_get_opcode(insn);
	opcode = insn->opcode.bytes[0];
	
	/* Handle 'ret' group to avoid marking scratch registers used for 
	 * these instructions. */
	if (opcode == 0xc3 || opcode == 0xc2 || 
	    opcode == 0xca || opcode == 0xcb)
		return X86_REG_MASK(INAT_REG_CODE_SP);
	
	reg_mask = insn_reg_mask(insn);
	dest = insn_jumps_to(insn);
	
	if (dest != 0 && 
	    (dest < start_addr || dest >= start_addr + func->size))
	    	reg_mask |= X86_REG_MASK_SCRATCH;
		
	return reg_mask;
}

/* Returns the code of a register which is in 'mask_choose_from' (the 
 * corresponding bit is 1) but not in 'mask_used' (the corresponding bit is 
 * 0). The code is 0-7 on x86-32 and 0-15 on x86-64. If there are several
 * registers of this kind, it is unspecified which one of them is returned.
 * If there are no such registers, 0xff is returned. 
 *
 * N.B. The higher bits of the masks must be cleared. */
static u8 
choose_register(unsigned int mask_choose_from, unsigned int mask_used)
{
	unsigned int mask;
	u8 rcode = 0;
	
	BUG_ON((mask_choose_from & ~X86_REG_MASK_ALL) != 0);
	BUG_ON((mask_used & ~X86_REG_MASK_ALL) != 0);
	
	/* N.B. Both masks have their higher bits zeroed => so will 'mask'*/
	mask = mask_choose_from & ~mask_used;
	if (mask == 0)
		return 0xff; /* nothing found */
	
	while (mask % 2 == 0) {
		mask >>= 1;
		++rcode;
	}
	return rcode;
}

static u8 
choose_work_register(unsigned int mask_choose_from, unsigned int mask_used, 
	u8 base)
{
	return choose_register(mask_choose_from, 
		mask_used | X86_REG_MASK(base));
}
/* ====================================================================== */

struct kedr_ir_node *
kedr_ir_node_create(void)
{
	struct kedr_ir_node *node;
	
	node = kzalloc(sizeof(struct kedr_ir_node), GFP_KERNEL);
	if (node == NULL)
		return NULL;
	
	node->first_node = node;
	node->last_node  = node;
	
	return node;
}

void
kedr_ir_node_destroy(struct kedr_ir_node *node)
{
	kfree(node);
}

/* Construct an IR node from the decoded instruction '*src_insn'. 
 * The instruction is copied to the node. The function returns the pointer 
 * to the constructed and initialized node on success, NULL if there is not
 * enough memory to complete the operation. 
 * 
 * The function sets 'orig_addr' field of the newly created node as the 
 * value of 'src_insn->kaddr', the address of the original instruction. */
static struct kedr_ir_node *
ir_node_create_from_insn(const struct insn *src_insn)
{
	struct kedr_ir_node *node;
	
	BUG_ON(src_insn == NULL);
	
	/* If src_insn->length is 0, this means that '*src_insn' instruction
	 * is not decoded completely, which must not happen here. */
	BUG_ON(src_insn->length == 0);
	BUG_ON(src_insn->length > X86_MAX_INSN_SIZE);
	
	node = kedr_ir_node_create();
	if (node == NULL)
		return NULL;
	
	/* Copy the instruction bytes */
	memcpy(&node->insn_buffer[0], src_insn->kaddr, src_insn->length);
	
	/* Copy the decoded information, adjust the pointers */
	memcpy(&node->insn, src_insn, sizeof(struct insn));
	node->insn.kaddr = (insn_byte_t *)(&node->insn_buffer[0]);
	node->insn.next_byte = 
		(insn_byte_t *)((unsigned long)(&node->insn_buffer[0]) +
		src_insn->length);
	
	node->orig_addr = (unsigned long)src_insn->kaddr;
	return node;
}

/* Remove all the nodes from the IR and delete all of them. */
static void
ir_destroy(struct list_head *ir)
{
	struct kedr_ir_node *pos;
	struct kedr_ir_node *tmp;
	
	list_for_each_entry_safe(pos, tmp, ir, list) {
		list_del(&pos->list);
		kedr_ir_node_destroy(pos);
	}
}

/* For each direct jump within the function, link its node in the IR to the 
 * node corresponding to the destination. */
static void
ir_make_links_for_jumps(struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_ir_node *pos;

	WARN_ON(list_empty(ir));
	
	/* [NB] the 0 address is definitely outside of the function */
	list_for_each_entry(pos, ir, list) {
		if (!is_address_in_function(pos->dest_addr, func))
			continue;
		pos->dest_inner = node_map_lookup(pos->dest_addr);
		
		/* If the jump destination is inside of this function, we 
		 * must have created the node for it and add this node to
		 * the hash map. */
		if (pos->dest_inner == NULL) {
			pr_err("[sample] No IR element found for "
				"the instruction at %p\n", 
				(void *)pos->dest_addr);
			BUG();
		}
	}
}

/* ====================================================================== */

/* The structure used to pass the required data to the instruction 
 * processing facilities (invoked by for_each_insn_in_function() in 
 * instrument_function() - hence "if_" in the name). 
 * The structure should be kept reasonably small in size so that it could be 
 * placed on the stack. */
struct kedr_if_data
{
	struct module *mod;   /* target module */
	struct list_head *ir; /* intermediate representation of the code */
	// TODO: add more fields here if necessary.
};

static int
skip_trailing_zeros(struct kedr_ifunc *func)
{
	/* Skip trailing zeros. If these are a part of an instruction,
	 * it will be handled automatically. If it just a padding sequence,
	 * we will avoid reading past the end of the function.
	 * Anyway, it is unlikely that a function ends with something like
	 * 'add %al, %(eax)', that is, 0x0000. */
	while (func->size != 0 && 
		*(u8 *)((unsigned long)func->addr + func->size - 1) == '\0')
		--func->size;
	
	if (func->size == 0) { /* Very unlikely. Broken module? */
		pr_err("[sample] "
"A spurious symbol \"%s\" (address: %p) seems to contain only zeros\n",
			func->name,
			func->addr);
		return -EILSEQ;
	}
	return 0;
}

/* Return nonzero if the given tables overlap, 0 otherwise. */
static int 
jtables_overlap(struct kedr_jtable *jtable1, struct kedr_jtable *jtable2)
{
	if (jtable2->addr <= jtable1->addr) {
		unsigned long jtable2_end = (unsigned long)jtable2->addr + 
			jtable2->num * sizeof(unsigned long);
		return (jtable2_end > (unsigned long)jtable1->addr);
	} 
	else { /* jtable2->addr > jtable1->addr */
		unsigned long jtable1_end = (unsigned long)jtable1->addr + 
			jtable1->num * sizeof(unsigned long);
		return (jtable1_end > (unsigned long)jtable2->addr);
	}
}

/* Check if this jump table and some jump tables processed earlier overlap,
 * and if so, adjust numbers of elements as necessary to eliminate this. 
 * 
 * Call this function before adding jtable to the list of jump tables 
 * in 'func'. */
static void
resolve_jtables_overlaps(struct kedr_jtable *jtable, 
	struct kedr_ifunc *func)
{
	struct kedr_jtable *pos;
	list_for_each_entry(pos, &func->jump_tables, list) {
		if (!jtables_overlap(jtable, pos))
			continue;
		
		/* Due to the way the tables are searched for, they must end
		 * at the same address if they overlap. 
		 * 
		 * [NB] When adding, we take into account that *->addr is 
		 * a pointer to unsigned long. */
		WARN_ON(jtable->addr + jtable->num != pos->addr + pos->num);
		
		if (jtable->addr == pos->addr) {
			jtable->num = 0;
		} 
		else if (pos->addr < jtable->addr) {
			pos->num = (unsigned int)((int)pos->num - 
				(int)jtable->num);
		}
		else { /* jtable->addr < pos->addr */
			jtable->num = (unsigned int)((int)jtable->num - 
				(int)pos->num);
		}
	}
}

static int 
handle_jmp_near_indirect(struct kedr_ifunc *func, struct insn *insn, 
	struct kedr_if_data *if_data)
{
	unsigned long jtable_addr;
	unsigned long end_addr = 0;
	int in_init = 0;
	int in_core = 0;
	struct module *mod = if_data->mod;
	unsigned long pos;
	unsigned int num_elems;
	struct kedr_jtable *jtable;
	
	jtable_addr = 
		X86_SIGN_EXTEND_V32(insn->displacement.value);
	
	/* [NB] Do not use is_*_text_address() here, because the jump tables
	 * are usually stored in one of the data sections rather than code
	 * sections. */
	if (is_core_address(jtable_addr, mod)) {
		in_core = 1;
		end_addr = (unsigned long)mod->module_core + 
			(unsigned long)mod->core_size - 
			sizeof(unsigned long);
	}
	else if (is_init_address(jtable_addr, mod)) {
		in_init = 1;
		end_addr = (unsigned long)mod->module_init + 
			(unsigned long)mod->init_size - 
			sizeof(unsigned long);
	}
	
	/* Sanity check: jtable_addr should point to some location within
	 * the module. */
	if (!in_core && !in_init) {
		pr_warning("[sample] Spurious jump table (?) at %p "
			"referred to by jmp at %pS, leaving it as is.\n",
			(void *)jtable_addr,
			insn->kaddr);
		WARN_ON_ONCE(1);
		return 0;
	}
	
	/* A rather crude (and probably not always reliable) way to find
	 * the number of elements in the jump table. */
	num_elems = 0;
	for (pos = jtable_addr; pos <= end_addr; 
		pos += sizeof(unsigned long)) {
		unsigned long jaddr = *(unsigned long *)pos;
		if (!is_address_in_function(jaddr, func))
			break;
		++num_elems;
	}
	
	/* Store the information about this jump table in 'func'. It may be
	 * needed during instrumentation to properly fixup the contents of
	 * the table. */
	jtable = (struct kedr_jtable *)kzalloc(
		sizeof(struct kedr_jtable), GFP_KERNEL);
	if (jtable == NULL)
		return -ENOMEM;
	
	jtable->addr = (unsigned long *)jtable_addr;
	jtable->num  = num_elems;
	
	resolve_jtables_overlaps(jtable, func);
	
	/* We add the new item at the tail of the list to make sure the 
	 * order of the items is the same as the order of the corresponding
	 * indirect jumps. This simplifies creation of the jump tables for 
	 * the instrumented instance of the function. */
	list_add_tail(&jtable->list, &func->jump_tables);
	++func->num_jump_tables;
	
	//<>
	pr_info("[DBG] Found jump table with %u entries at %p "
		"referred to by a jmp at %pS\n",
		jtable->num,
		(void *)jtable->addr, 
		(void *)insn->kaddr);
	//<>
	return 0;
}

static void
ir_node_set_dest_addr(struct kedr_ir_node *node, struct insn *insn)
{
	node->dest_addr = insn_jumps_to(insn);
}

static void
ir_node_set_iprel_addr(struct kedr_ir_node *node, struct insn *insn)
{
#ifdef CONFIG_X86_64
	if (!insn_rip_relative(insn))
		return;

	node->iprel_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
		insn->kaddr,
		insn->length, 
		insn->displacement.value);
#endif
}

static int
do_process_insn(struct kedr_ifunc *func, struct insn *insn, void *data)
{
	int ret = 0;
	struct kedr_if_data *if_data = (struct kedr_if_data *)data;
	unsigned long start_addr;
	unsigned long offset_after_insn;
	u8 opcode;
	struct kedr_ir_node *node = NULL;
	
	BUG_ON(if_data == NULL);
	
	start_addr = (unsigned long)func->addr;
	offset_after_insn = (unsigned long)insn->kaddr + 
		(unsigned long)insn->length - start_addr;
		
	/* If we've got too far, probably there is a bug in our system. It 
	 * is impossible for an instruction to be located at 64M distance
	 * or further from the beginning of the corresponding function. */
	BUG_ON(offset_after_insn >= 0x04000000UL);
	
	/* If we have skipped too many zeros at the end of the function, 
	 * that is, if we have cut off a part of the last instruction, fix
	 * it now. */
	if (offset_after_insn > func->size)
		func->size = offset_after_insn;
	
	/* Create the IR node and record the mapping (address, node) in a 
	 * hash map. */
	node = ir_node_create_from_insn(insn);
	if (node == NULL)
		return -ENOMEM;
	
	ir_node_set_dest_addr(node, insn);
	ir_node_set_iprel_addr(node, insn);
	
	list_add_tail(&node->list, if_data->ir);
	node_map_add(node);
	
	/* Process indirect near jumps that can use jump tables, namely
	 * the jumps having the following form: 
	 * jmp near [<jump_table> + reg * <scale>]. 
	 * [NB] We don't need to do anything about other kinds of indirect 
	 * jumps, like jmp near [reg], here. 
	 * 
	 * jmp near indirect has code FF/4. 'mod' and 'R/M' fields are used 
	 * here to determine if SIB byte is present. */
	opcode = insn->opcode.bytes[0];
	if (opcode == 0xff && 
		X86_MODRM_REG(insn->modrm.value) == 4 && 
		X86_MODRM_MOD(insn->modrm.value) != 3 &&
		X86_MODRM_RM(insn->modrm.value) == 4) {
		ret = handle_jmp_near_indirect(func, insn, if_data);
		if (ret != 0)
			return ret;
	}
	
	return 0; 
}

/* Find the IR nodes corresponding to the elements of 'jtable', write their 
 * addresses to the elements of 'i_jtable'. The jump tables for the 
 * instrumented code will contain these addresses until the instrumented 
 * code is prepared. After that, the elements of these tables should be 
 * replaced with the appropriate values. 
 * 
 * This function also marks the appropriate IR nodes as the start nodes of 
 * the blocks. */
static void
ir_prefill_jump_table(const struct kedr_jtable *jtable, 
	unsigned long *i_jtable)
{
	unsigned int i;
	for (i = 0; i < jtable->num; ++i) {
		struct kedr_ir_node *node = node_map_lookup(jtable->addr[i]);
		i_jtable[i] = (unsigned long)node;
		if (i_jtable[i] == 0) {
			pr_err("[sample] No IR element found for "
				"the instruction at %p\n", 
				(void *)(jtable->addr[i]));
			BUG();
		}
		node->block_starts = 1;
	}
}

/* Creates the jump tables for the instrumented instance of the function 
 * 'func' based on the jump tables for the original function. The jump 
 * tables will be filled with meaningful data during the instrumentation. 
 * For now, they will be just allocated, and filled with the addresses of 
 * the corresponding IR nodes for future processing. These IR nodes will be
 * marked as the starting nodes of the code blocks among other things.
 * 
 * The pointers to the created jump tables will be stored in 
 * func->i_jump_tables[]. If an item of jump_table list has 0 elements, 
 * the corresponding item in func->i_jump_tables[] will be NULL.
 *
 * [NB] The order of the corresponding indirect jumps and the order of the 
 * elements in func->jump_tables list must be the same. 
 *
 * [NB] In case of error, func->i_jump_tables will be freed in 
 * ifunc_destroy(), so it is not necessary to free it here. */
static int 
create_jump_tables(struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_jtable *jtable;
	unsigned int total = 0;
	unsigned int i = 0;
	void *buf;
	
	if (func->num_jump_tables == 0)
		return 0;
		
	func->i_jump_tables = kzalloc(
		func->num_jump_tables * sizeof(unsigned long *),
		GFP_KERNEL);
	if (func->i_jump_tables == NULL)
		return -ENOMEM;
	
	/* Find the total number of elements in all jump tables for this 
	 * function. */
	list_for_each_entry(jtable, &func->jump_tables, list) {
		total += jtable->num;
	}
	
	/* If there are jump tables but each of these jump tables has no
	 * elements (i.e. the jumps are not within the function), nothing to
	 * do. */
	if (total == 0)
		return 0;
	
	buf = kedr_alloc_detour_buffer(total * sizeof(unsigned long));
	if (buf == NULL)
		return -ENOMEM;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		if (jtable->num == 0) {
			func->i_jump_tables[i] = NULL;
		} 
		else {
			func->i_jump_tables[i] = (unsigned long *)buf;
			buf = (void *)((unsigned long)buf + 
				jtable->num * sizeof(unsigned long));
			ir_prefill_jump_table(jtable, 
				func->i_jump_tables[i]);
		}
		++i;
	}
	BUG_ON(i != func->num_jump_tables);
	return 0;
}

/* Mark the node to indicate it is a separate block. */
static void
ir_mark_node_separate_block(struct kedr_ir_node *node, 
	struct list_head *ir_head)
{
	struct kedr_ir_node *node_after;
	
	node->block_starts = 1;
	if (node->list.next == ir_head) /* no nodes follow */
		return;

	node_after = list_entry(node->list.next, struct kedr_ir_node, list);
	node_after->block_starts = 1;
}

/* If the current instruction is a control transfer instruction, determine
 * whether it should be reflected in the set of code blocks (i.e. whether we
 * should mark some IR nodes as the beginnings of the blocks). 
 *
 * N.B. Call this function only for the nodes already added to the IR 
 * because the information about the instruction following this one may be 
 * needed. */
static void
ir_node_set_block_starts(struct kedr_ir_node *node, struct list_head *ir, 
	struct kedr_ifunc *func)
{
	/* The node should have been added to the IR before this function is 
	 * called. */
	BUG_ON(node->list.next == NULL);
	
	if (node->dest_addr == 0) /* not a control transfer instruction */
		return;
	
	if (is_address_in_function(node->dest_addr, func)) {
		BUG_ON(node->dest_inner == NULL); 
		if (node->dest_inner->orig_addr < node->orig_addr) {
			/* jump backwards */
			ir_mark_node_separate_block(node, ir);
			node->dest_inner->block_starts = 1;
		}
		
	} else	/* indirect jump or control transfer outside of the
		 * function => this instruction is a separate block */
		ir_mark_node_separate_block(node, ir);
}

/* Split the code into blocks (see the comment at the beginning of this 
 * file) and mark each nodes corresponding to the start of a block 
 * accordingly. 
 * Note that jump tables are not processed here but rather in 
 * create_jump_tables(). ir_find_blocks() should be called after that 
 * function because splitting the blocks having more than 
 * KEDR_MEM_NUM_RECORDS instructions accessing memory should be performed 
 * last. */
static void 
ir_mark_blocks(struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_ir_node *pos;
	int num_mem_ops = 0;
	
	BUG_ON(list_empty(ir));
	pos = list_first_entry(ir, struct kedr_ir_node, list);
	pos->block_starts = 1;
	
	/* The first pass: process control transfer instructions */
	list_for_each_entry(pos, ir, list)
		ir_node_set_block_starts(pos, ir, func);
	
	/* The second pass: split the blocks with more than 
	 * KEDR_MEM_NUM_RECORDS memory accessing instructions. */
	list_for_each_entry(pos, ir, list) {
		if (pos->block_starts)
			num_mem_ops = 0;
		
		if (num_mem_ops == KEDR_MEM_NUM_RECORDS) {
			/* Already found KEDR_MEM_NUM_RECORDS instructions 
			 * accessing memory in the current block. Start a
			 * new block then. */
			pos->block_starts = 1;
			num_mem_ops = 0;
		}
		
		if (insn_is_mem_read(&pos->insn) || 
		    insn_is_mem_write(&pos->insn))
			++num_mem_ops;
	}
}

/* ====================================================================== */

/* Using the IR created before, perform the instrumentation. */
static int
do_instrument(struct kedr_ifunc *func, struct list_head *ir)
{
	u32 *poffset;
	
	BUG_ON(func == NULL);
	BUG_ON(func->tbuf_addr != NULL);
	BUG_ON(func->num_jump_tables > 0 && func->i_jump_tables == NULL);
	
	//<>
	// For now, the instrumented instance will contain only a jump to 
	// the fallback function (relocated to the instruction address of 0)
	// to check the mechanism.
	func->tbuf_addr = kzalloc(KEDR_SIZE_JMP_REL32, GFP_KERNEL);
	if (func->tbuf_addr == NULL)
		return -ENOMEM;
	
	func->i_size = KEDR_SIZE_JMP_REL32;
	
	*(u8 *)func->tbuf_addr = KEDR_OP_JMP_REL32;
	poffset = (u32 *)((unsigned long)func->tbuf_addr + 1);
	*poffset = X86_OFFSET_FROM_ADDR(0, 
		KEDR_SIZE_JMP_REL32, 
		(unsigned long)func->fallback);
	//<>
	
	// TODO: replace this stub with a real instrumentation.
	return 0;
}
/* ====================================================================== */

int
instrument_function(struct kedr_ifunc *func, struct module *mod)
{
	int ret = 0;
	struct kedr_if_data if_data;
	
	/* The intermediate representation of a function's code. */
	struct list_head kedr_ir;
	INIT_LIST_HEAD(&kedr_ir);
	
	BUILD_BUG_ON(KEDR_MEM_NUM_RECORDS > sizeof(unsigned long) * 8);
	BUG_ON(func->size < KEDR_SIZE_JMP_REL32);
	
	ret = skip_trailing_zeros(func);
	if (ret != 0)
		return ret;
	
	node_map_init();
	
	/* First, decode and process the machine instructions one by one and
	 * build the IR, at this stage, without inter-node links. In 
	 * addition, the mapping (address of original insn; node) will be 
	 * prepared there.
	 * 
	 * do_process_insn() will also adjust the length of the function if
	 * we have skipped too many trailing zeros above. */
	if_data.mod = mod;
	if_data.ir = &kedr_ir;
	ret = for_each_insn_in_function(func, do_process_insn, 
		(void *)&if_data);
	if (ret != 0)
		goto out;
	
	ir_make_links_for_jumps(func, &kedr_ir);
	
	/* Allocate and partially initialize the jump tables for the 
	 * instrumented instance. 
	 * At this stage, the jump tables will be filled with pointers
	 * to the corresponding IR nodes rather than the instructions 
	 * themselves. 
	 * Later (see below), when the instrumented code has been prepared, 
	 * these addresses will be replaced with the offsets of the 
	 * appropriate instructions in that code (i.e. the addresses 
	 * relocated to the start address of the instrumented instance 
	 * being 0). */
	ret = create_jump_tables(func, &kedr_ir);
	if (ret != 0)
		goto out;
	
	ir_mark_blocks(func, &kedr_ir);
	
	/* Create the instrumented instance of the function. */
	ret = do_instrument(func, &kedr_ir);
	if (ret != 0)
		goto out;

	//<>
	if (strcmp(func->name, "cfake_read") == 0) {
		unsigned int mask_choose_from = X86_REG_MASK_ALL & ~X86_REG_MASK(INAT_REG_CODE_SP)
			& ~X86_REG_MASK(INAT_REG_CODE_SI);
		unsigned int mask_used = X86_REG_MASK_SCRATCH | X86_REG_MASK(INAT_REG_CODE_BP)
			| X86_REG_MASK(INAT_REG_CODE_DI);
		pr_info("[DBG] choose from: 0x%x, used: 0x%x, chosen: %u\n",
			mask_choose_from,
			mask_used,
			/*choose_register(mask_choose_from, mask_used)*/
			choose_work_register(mask_choose_from, mask_used, INAT_REG_CODE_BX)
		);
		
		pr_info("[DBG] size of primary storage (bytes): %zu\n",
			sizeof(struct kedr_primary_storage));
			
		pr_info("[DBG] function addresses: "
			"0x%lx, 0x%lx, 0x%lx, 0x%lx\n",
			(unsigned long)&kedr_process_function_entry_wrapper,
			(unsigned long)&kedr_process_function_exit_wrapper,
			(unsigned long)&kedr_process_block_end_wrapper,
			(unsigned long)&kedr_lookup_replacement_wrapper);
	}
	//<>
	
	//<>
	if (strcmp(func->name, target_function) == 0) {
		struct kedr_ir_node *node;
		list_for_each_entry(node, &kedr_ir, list) {
			unsigned long offset;
			if (!node->block_starts)
				continue;
			offset = node->orig_addr - (unsigned long)func->addr;
			debug_util_print_u64((u64)offset, "0x%llx\n");
		}
	}
	//<>

out:
	node_map_clear();
	ir_destroy(&kedr_ir);
	return ret;
}
/* ====================================================================== */
