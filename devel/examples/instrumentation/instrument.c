/* instrument.c - instrumentation-related facilities. */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/hash.h>
#include <linux/module.h>

#include <kedr/asm/insn.h> /* instruction analysis support */

#include "instrument.h"
#include "util.h"
#include "detour_buffer.h"
#include "primary_storage.h"
#include "ir.h"
#include "operations.h"
#include "ir_handlers.h"

//<> For debugging only
#include "debug_util.h"
extern char *target_function;
const char *func_name = "";
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

/* A special code with the meaning "no register". */
#define KEDR_REG_NONE   0xff

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

//static int 
//add_relocation(struct kedr_ifunc *func, ??? struct kedr_ir_node *node, 
//	void *destination)
//{
//	struct kedr_reloc *reloc;
//	reloc = kzalloc(sizeof(struct kedr_reloc), GFP_KERNEL);
//	
//	if (reloc == NULL)
//		return -ENOMEM;
//	
//	reloc->offset = ???;
//	reloc->dest = destination;
//	list_add_tail(&reloc->list, &func->relocs);
//	
//	return 0;
//}
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
 * If there are no such registers, KEDR_REG_NONE is returned. 
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
		return KEDR_REG_NONE; /* nothing found */
	
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
	
	node->first = node;
	node->last  = node;
	
	node->reg_mask = X86_REG_MASK_ALL;
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
 * value of 'src_insn->kaddr', the address of the original instruction. 
 * 'dest_addr' is also set. */
static struct kedr_ir_node *
ir_node_create_from_insn(struct insn *src_insn)
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
	node->dest_addr = insn_jumps_to(src_insn);
	
	return node;
}

/* Remove all the nodes from the IR and destroy them. */
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

/* Non-zero if the node corresponded to an instruction from the original
 * function when that node was created, that is, if it is a reference node.
 * 0 is returned otherwise. */
static int
is_reference_node(struct kedr_ir_node *node)
{
	return (node->orig_addr != 0);
}

/* For each direct jump within the function, link its node in the IR to the 
 * node corresponding to the destination. */
static void
ir_make_links_for_jumps(struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_ir_node *pos;

	WARN_ON(list_empty(ir));
	
	/* [NB] the address 0 is definitely outside of the function */
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

/* See the description of kedr_ir_node::iprel_addr */
static void
ir_node_set_iprel_addr(struct kedr_ir_node *node, struct kedr_ifunc *func)
{
	u8 opcode = node->insn.opcode.bytes[0];
	if (opcode == KEDR_OP_CALL_REL32 || opcode == KEDR_OP_JMP_REL32) {
		BUG_ON(node->dest_addr == 0);
		BUG_ON(node->dest_addr == (unsigned long)(-1));
		
		if (!is_address_in_function(node->dest_addr, func))
			node->iprel_addr = node->dest_addr;
		return;
	}
	
#ifdef CONFIG_X86_64
	/* For the instructions with IP-relative addressing, we also check
	 * if they refer to something inside the original function. If so,
	 * a warning is issued (such situations need more investigation). */
	if (insn_rip_relative(&node->insn)) {
		node->iprel_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			node->orig_addr,
			node->insn.length, 
			node->insn.displacement.value);

		if (is_address_in_function(node->iprel_addr, func)) {
			pr_info("[sample] Warning: the instruction at %pS "
				"uses IP-relative addressing to access "
				"the code of the original function.\n",
				(void *)node->orig_addr);
			WARN_ON_ONCE(1);
		}
	}
#endif
	/* node->iprel_addr remains 0 by default */
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
process_jmp_near_indirect(struct kedr_ifunc *func, 
	struct kedr_if_data *if_data, struct kedr_ir_node *node)
{
	unsigned long jtable_addr;
	unsigned long end_addr = 0;
	int in_init = 0;
	int in_core = 0;
	struct module *mod = if_data->mod;
	struct insn *insn = &node->insn;
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
			(void *)node->orig_addr);
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
	jtable->referrer = node;
	
	resolve_jtables_overlaps(jtable, func);
	
	/* We add the new item at the tail of the list to make sure the 
	 * order of the items is the same as the order of the corresponding
	 * indirect jumps. This simplifies creation of the jump tables for 
	 * the instrumented instance of the function. */
	list_add_tail(&jtable->list, &func->jump_tables);
	
	//<>
	pr_info("[DBG] Found jump table with %u entries at %p "
		"referred to by a jmp at %pS\n",
		jtable->num,
		(void *)jtable->addr, 
		(void *)node->orig_addr);
	//<>
	return 0;
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
	
	/* [NB] We cannot skip the no-ops as they may be the destinations of
	 * the jumps. 
	 * For example, PAUSE instruction (F3 90) is a special kind of a nop
	 * that is used inside the spin-wait loops, jumps to it are common. 
	 */
	
	/* Create and initialize the IR node and record the mapping 
	 * (address, node) in the hash map. */
	node = ir_node_create_from_insn(insn);
	if (node == NULL)
		return -ENOMEM;
	
	ir_node_set_iprel_addr(node, func);
	
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
		ret = process_jmp_near_indirect(func, if_data, node);
		if (ret != 0)
			return ret;
	}
	return 0; 
}

/* Find the IR nodes corresponding to the elements of 'jtable', write their 
 * addresses to the elements of 'jtable->i_table'. The jump tables for the 
 * instrumented code will contain these addresses until the instrumented 
 * code is prepared. After that, the elements of these tables should be 
 * replaced with the appropriate values. 
 * 
 * This function also marks the appropriate IR nodes as the start nodes of 
 * the blocks. */
static void
ir_prefill_jump_table(const struct kedr_jtable *jtable)
{
	unsigned long *table = jtable->i_table;
	unsigned int i;
	for (i = 0; i < jtable->num; ++i) {
		struct kedr_ir_node *node = node_map_lookup(jtable->addr[i]);
		table[i] = (unsigned long)node;
		if (table[i] == 0) {
			pr_err("[sample] No IR element found for "
				"the instruction at %p\n", 
				(void *)(jtable->addr[i]));
			BUG();
		}
		node->block_starts = 1;
	}
}

static unsigned long
find_i_table(struct kedr_jtable *jtable, struct list_head *jt_list)
{
	struct kedr_jtable *pos;
	
	if (jtable->i_table != NULL)
		return (unsigned long)jtable->i_table;
	
	BUG_ON(jtable->num != 0);
	
	/* 'jtable' seems to have no elements. Find if there is another 
	 * instance of struct kedr_jtable that refers to the same jump table
	 * but has nonzero elements. This would mean that two or more jumps
	 * in the function use the same jump table. Very unlikely, but 
	 * still. */
	list_for_each_entry(pos, jt_list, list) {
		if (pos != jtable && pos->addr == jtable->addr && 
		    pos->i_table != NULL) {
		    	return (unsigned long)pos->i_table;
		}
	}
	return 0; /* A really empty jump table. */
}

/* Sets the addresses of the jump tables in the IR nodes corresponding to
 * the indirect near jumps. That is, the function replaces 'disp32' in these 
 * jumps with the lower 32 bits of the jump table addresses to be used in 
 * the instrumented code. After that, this displacement should remain the
 * same during the rest of the instrumentation process.
 * 
 * [NB] The (unlikely) situation when 2 or more jumps use the same jump
 * table is handled here too. 
 * 
 * [NB] The jumps with "empty" jump tables will remain unchanged. */
static void
ir_set_jtable_addresses(struct kedr_ifunc *func)
{
	struct kedr_jtable *jtable;
	unsigned long table;
	u8 *pos;
	struct kedr_ir_node *node;
	unsigned char len;
	
	if (list_empty(&func->jump_tables))
		return; 
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		BUG_ON(jtable->referrer == NULL);
		node = jtable->referrer;
		
		table = find_i_table(jtable, &func->jump_tables);
		if (table == 0) 
			continue;
		
		pos = node->insn_buffer + 
			insn_offset_displacement(&node->insn);
		len = node->insn.length;
		*(u32 *)pos = (u32)table; 
		/* On x86-64, the bits we have cut off from the address of 
		 * the table must all be 1, because the table resides in
		 * the module mapping space. */
		
		/* Re-decode the instruction - just in case. */
		kernel_insn_init(&node->insn, (void *)node->insn_buffer);
		insn_get_length(&node->insn);
		
		BUG_ON(len != node->insn.length);
	}
}

/* Creates the jump tables for the instrumented instance of the function 
 * 'func' based on the jump tables for the original function. The jump 
 * tables will be filled with meaningful data during the instrumentation. 
 * For now, they will be just allocated, and filled with the addresses of 
 * the corresponding IR nodes for future processing. These IR nodes will be
 * marked as the starting nodes of the code blocks among other things.
 * 
 * The pointers to the created jump tables will be stored in 'i_table' 
 * fields of the corresponding jtable structures. If an item of jump_table 
 * list has 0 elements, jtable->i_table will be NULL.
 *
 * [NB] The order of the corresponding indirect jumps and the order of the 
 * elements in func->jump_tables list must be the same. 
 *
 * [NB] In case of error, func->jt_buf will be freed in ifunc_destroy(), so 
 * it is not necessary to free it here. */
static int 
create_jump_tables(struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_jtable *jtable;
	unsigned int total = 0;
	void *buf;
		
	/* Find the total number of elements in all jump tables for this 
	 * function. */
	list_for_each_entry(jtable, &func->jump_tables, list) {
		total += jtable->num;
	}
	
	/* If there are no jump tables or each of the jump tables has no
	 * elements (i.e. the jumps are not within the function), nothing
	 * to do. */
	if (total == 0)
		return 0;
	
	buf = kedr_alloc_detour_buffer(total * sizeof(unsigned long));
	if (buf == NULL)
		return -ENOMEM;
	func->jt_buf = buf;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		if (jtable->num == 0) 
			continue;
		jtable->i_table = (unsigned long *)buf;
		buf = (void *)((unsigned long)buf + 
			jtable->num * sizeof(unsigned long));
		ir_prefill_jump_table(jtable);
	}

	ir_set_jtable_addresses(func);
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


/* Can the instruction in the node transfer control outside of the given
 * function? If not, the return value is 0. If it can or it is unknown 
 * (e.g., indirect jumps), the return value is non-zero. */
static int
is_transfer_outside(struct kedr_ir_node *node, struct kedr_ifunc *func)
{
	return (node->dest_addr != 0 && 
		!is_address_in_function(node->dest_addr, func));
}

/* An instruction constitutes a special (as opposed to "normal") block 
 * if it transfers control outside of the function or is a jump backwards
 * within the function. Indirect jumps and calls are also considered as 
 * special blocks. */
static int 
is_special_block(struct kedr_ir_node *node, struct kedr_ifunc *func)
{
	return (is_transfer_outside(node, func) ||
		(node->dest_inner != NULL && 
			node->dest_inner->orig_addr <= node->orig_addr));
	/* "<=" is here rather than plain "<" just in case a jump to itself 
	 * is encountered. I have seen such jumps a couple of times in the 
	 * kernel modules, some special kind of padding, may be. */
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
	
	if (is_special_block(node, func)) {
		ir_mark_node_separate_block(node, ir);
		if (node->dest_inner != NULL)
			node->dest_inner->block_starts = 1;
	}
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

//<>
/* TODO: remove when the instrumentation mechanism is prepared. */
static int
stub_do_instrument(struct kedr_ifunc *func, struct list_head *ir)
{
	u32 *poffset;
	struct kedr_reloc *reloc;
	
	/* For now, the instrumented instance will contain only a jump to 
	 * the fallback function to check the mechanism. */
	func->tbuf = kzalloc(KEDR_SIZE_JMP_REL32, GFP_KERNEL);
	if (func->tbuf == NULL)
		return -ENOMEM;
	
	reloc = kzalloc(sizeof(struct kedr_reloc), GFP_KERNEL);
	if (reloc == NULL) {
		kfree(func->tbuf);
		func->tbuf = NULL;
		return -ENOMEM;
	}
	reloc->offset = 0; /* The insn is at the beginning of the buffer */
	reloc->dest = func->fallback;
	list_add_tail(&reloc->list, &func->relocs);
	
	func->i_size = KEDR_SIZE_JMP_REL32;
	
	*(u8 *)func->tbuf = KEDR_OP_JMP_REL32;
	poffset = (u32 *)((unsigned long)func->tbuf + 1);
	*poffset = 0; /* to be relocated during deployment */
	
	return 0;
}
//<>

#ifdef CONFIG_X86_64
static unsigned int 
update_base_mask_for_string_insn(struct kedr_ir_node *node, 
	unsigned int base_mask)
{
	/* %rsi and %rdi are scratch registers on x86-64, so they cannot be
	 * used as a base register anyway. No special handling of string
	 * instructions is necessary here. */
	return base_mask;
}

static int
is_pushad(struct insn *insn)
{
	/* No "PUSHAD" instruction on x86-64. */
	return 0;
}

static int
is_popad(struct insn *insn)
{
	/* No "POPAD" instruction on x86-64. */
	return 0;
}

#else	/* CONFIG_X86_32 */
static unsigned int 
update_base_mask_for_string_insn(struct kedr_ir_node *node, 
	unsigned int base_mask)
{
	/* If the function contains instructions with addressing method X
	 * (movs, lods, ...), %esi cannot be used as a base register. 
	 * Same for addressing method Y (movs, stos, ...) and %edi. */
	insn_attr_t *attr = &node->insn.attr;
	if (attr->addr_method1 == INAT_AMETHOD_X || 
	    attr->addr_method2 == INAT_AMETHOD_X)
		base_mask &= ~X86_REG_MASK(INAT_REG_CODE_SI);
	
	if (attr->addr_method1 == INAT_AMETHOD_Y || 
	    attr->addr_method2 == INAT_AMETHOD_Y)
		base_mask &= ~X86_REG_MASK(INAT_REG_CODE_DI);
	
	return base_mask;
}

static int
is_pushad(struct insn *insn)
{
	BUG_ON(insn == NULL || insn->length == 0);
	return (insn->opcode.bytes[0] == 0x60);
}

static int
is_popad(struct insn *insn)
{
	BUG_ON(insn == NULL || insn->length == 0);
	return (insn->opcode.bytes[0] == 0x61);
}
#endif

/* Collects the data about register usage in the function and chooses 
 * the base register for the instrumentation of that function. 
 *
 * The function saves the collected data about register usage in 'reg_mask'
 * fields of the corresponding nodes.
 * 
 * The return value is the code of the base resister on success, a negative
 * error code on failure. */
static int
ir_choose_base_register(struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_ir_node *node;
	unsigned int allowed_base_mask = X86_REG_MASK_NON_SCRATCH;
	unsigned int reg_usage[X86_REG_COUNT];
	int base = KEDR_REG_NONE;
	unsigned int usage_count = UINT_MAX;
	int i;
	
	memset(&reg_usage[0], 0, sizeof(reg_usage));
	
	list_for_each_entry(node, ir, list) {
		unsigned int mask;
		
		allowed_base_mask = update_base_mask_for_string_insn(node,
			allowed_base_mask);
		mask = register_usage_mask(&node->insn, func);
		BUG_ON(mask > X86_REG_MASK_ALL);
		
		if ((mask == X86_REG_MASK_ALL) && 
		    !is_pushad(&node->insn) && !is_popad(&node->insn)) {
		    	/* Of all the instructions using all registers, we
		    	 * can handle PUSHAD and POPAD only. */
			pr_warning("[sample] The instruction at %pS seems "
			"to use all general-purpose registers and is "
			"neither PUSHAD nor POPAD. Currently, we can not "
			"instrument the modules containing such "
			"instructions.\n",
				(void *)node->orig_addr);
			return -EILSEQ;
		}
		
		node->reg_mask = mask;
		for (i = 0; i < X86_REG_COUNT; ++i) {
			if (mask & X86_REG_MASK(i))
				++reg_usage[i];
		}
	}
	
	for (i = 0; i < X86_REG_COUNT; ++i) {
		if ((X86_REG_MASK(i) & allowed_base_mask) &&
		    reg_usage[i] < usage_count) {
		    	base = i;
		    	usage_count = reg_usage[i];
		}
	}
	BUG_ON(base == KEDR_REG_NONE); /* we should have chosen something */
	
	//<>
	pr_warning("[DBG] allowed_base_mask = 0x%08x; "
		"chosen: %d (usage count: %u)\n",
		allowed_base_mask, base, usage_count);
	//<>
	return base;
}

/* Tests if the node corresponds to 'jmp near indirect'
 * Opcode: FF/4 */
static int
is_jump_near_indirect(struct kedr_ir_node *node)
{
	struct insn *insn = &node->insn;
	return (insn->opcode.bytes[0] == 0xff && 
		X86_MODRM_REG(insn->modrm.bytes[0]) == 4);
}

/* Tests if the node corresponds to 'call near indirect'
 * Opcode: FF/2 */
static int
is_call_near_indirect(struct kedr_ir_node *node)
{
	struct insn *insn = &node->insn;
	return (insn->opcode.bytes[0] == 0xff && 
		X86_MODRM_REG(insn->modrm.bytes[0]) == 2);
}

/* Calls: E8; 9A; FF/2, FF/3 */
static int
insn_is_call(struct insn *insn)
{
	u8 opcode = insn->opcode.bytes[0];
	u8 ext_opcode = X86_MODRM_REG(insn->modrm.bytes[0]);
	
	return (opcode == 0xe8 || opcode == 0x9a ||
		(opcode == 0xff && (ext_opcode == 2 || ext_opcode == 3)));
}

/* Each control transfer outside of the function that is not a call or 
 * an indirect jump is considered a function exit here. 
 * Indirect jumps will be handled separately because we can only determine
 * the destination of such jumps in runtime. */
static int
is_function_exit(struct kedr_ir_node *node, struct kedr_ifunc *func)
{
	struct insn *insn = &node->insn;
	return (is_transfer_outside(node, func) &&
		!insn_is_call(insn) &&
		!is_jump_near_indirect(node));
}

static int
is_end_of_normal_block(struct list_head *ir, struct kedr_ir_node *node, 
	struct kedr_ifunc *func)
{
	struct kedr_ir_node *next_node;
	
	if (is_special_block(node, func) || 
	    node->list.next == ir) /* leave the padding alone */
		return 0;
	
	next_node = list_entry(node->list.next, struct kedr_ir_node, list);
	return next_node->block_starts;
}

static int
is_jump_out_of_block(struct list_head *ir, struct kedr_ir_node *node, 
	struct kedr_ifunc *func)
{
	struct kedr_ir_node *pos;
	
	/* We may consider only inner jumps forward. Other kinds of control 
	 * transfer can only be performed by the instructions in the special
	 * blocks. */
	if (node->dest_inner == 0 || is_special_block(node, func))
		return 0;
	
	/* If we've got here, the node corresponds to an inner jump forward.
	 * Its destination must be before the end of the function. 
	 * In particular, the node must not be the last one. */
	BUG_ON(node->list.next == ir);
	pos = list_entry(node->list.next, struct kedr_ir_node, list);
	list_for_each_entry_from(pos, ir, list) {
		if (pos->block_starts) {
			/* Another block starts before the destination of 
			 * the jump (or the first its node may be that
			 * destination). */
			node->jump_past_block_end = 1;
			return 1;
		}
		
		/* We are still inside the same block and 'pos' is the 
		 * destination of the jump. */
		if (pos == node->dest_inner)
			return 0;
	}
	
	/* Must not get here because if we do, it means that neither the 
	 * next block nor the destination of the jump has been found before
	 * the end of the function. */
	BUG();
	return 0;
}

/* Using the IR created before, perform the instrumentation. */
static int
do_instrument(struct kedr_ifunc *func, struct list_head *ir)
{
	int ret;
	u8 base;
	struct kedr_ir_node *node;
	struct kedr_ir_node *tmp;
	
	BUG_ON(func == NULL);
	BUG_ON(func->tbuf != NULL);
	BUG_ON(!list_empty(&func->jump_tables) && func->jt_buf == NULL);
	
	//<> For debugging only
	func_name = func->name;
	//<>
	
	ret = ir_choose_base_register(func, ir);
	if (ret < 0)
		return ret;
	
	base = (u8)ret;
	
	/* Phase 1: "release" the base register and handle the structural
	 * elements (entry, exits, block ends, ...). */
	list_for_each_entry_safe(node, tmp, ir, list) {
		if (!is_reference_node(node))
			continue;
		
		ret = 0;
		if (is_function_exit(node, func))
			ret = kedr_handle_function_exit(node, base);
		else if (is_jump_out_of_block(ir, node, func))
			ret = kedr_handle_jump_out_of_block(node, base);
		else if (is_call_near_indirect(node))
			ret = kedr_handle_call_near_indirect(node, base);
		else if (is_jump_near_indirect(node))
			ret = kedr_handle_jump_near_indirect(node, base);
		else if (is_pushad(&node->insn))
			ret = kedr_handle_pushad(node, base);
		else if (is_popad(&node->insn))
			ret = kedr_handle_popad(node, base);
		else 
			/* General case, just "release" the base register. 
			 * This can be necessary for special blocks too. */
			ret = kedr_handle_general_case(node, base);
		
		if (ret < 0)
			return ret;
		
		/* In addition to handling the node, determine if it is the
		 * last node of a normal block and if so, add appropriate 
		 * instructions after it. */
		if (is_end_of_normal_block(ir, node, func)) {
			ret = kedr_handle_end_of_normal_block(node, base);
			if (ret < 0)
				return ret;
		}
	}
	ret = kedr_handle_function_entry(ir, func, base);
	if (ret < 0)
		return ret;
	
	// TODO
	// Phase 2: ...
	
	// TODO: Choose the length of the inner jumps
	
	// TODO: Create the temporary buffer and place the code there.
	// TODO: During that process, add relocations for the nodes with 
	// iprel_addr != 0 to func->relocs.
	
	// TODO: replace this stub with a real instrumentation.
	return stub_do_instrument(func, ir);
}
/* ====================================================================== */

/* If the node is the first in a block, includes the nodes created when 
 * processing the former into this block. Does nothing otherwise. */
static void
ir_node_update_block_start(struct kedr_ir_node *node)
{
	if (node->block_starts && node->first != node) {
		node->first->block_starts = 1;
		node->block_starts = 0;
	}
}
/* ====================================================================== */

/* If the instruction is jmp short, replace it with jmp near. 
 * The function does nothing if the node contains some other instruction. */
static void 
ir_node_jmp_short_to_near(struct kedr_ir_node *node)
{
	u8 opcode = node->insn.opcode.bytes[0];
	u8 *pos = node->insn_buffer;
	unsigned int offset_opcode;
	
	/* The function may be called only for the nodes corresponding
	 * to the original instructions. */
	BUG_ON(node->orig_addr == 0);
	
	if (opcode != 0xeb)
		return;

	/* Leave the prefixes intact if any are present. */	
	offset_opcode = insn_offset_opcode(&node->insn);
	pos += offset_opcode;
	
	*pos++ = KEDR_OP_JMP_REL32;
	
	/* Write the offset as if the instruction was in the original 
	 * instance of the function - just in case. */
	*(u32 *)pos = X86_OFFSET_FROM_ADDR(node->orig_addr, 
		offset_opcode + KEDR_SIZE_JMP_REL32,
		node->dest_addr);
	
	/* Re-decode the instruction. */
	kernel_insn_init(&node->insn, node->insn_buffer);
	insn_get_length(&node->insn);
	
	BUG_ON(node->insn.length != offset_opcode + KEDR_SIZE_JMP_REL32);
}

/* If the instruction is jcc short (conditional jump except jcxz), replace 
 * it with jcc near. 
 * The function does nothing if the node contains some other instruction. */
static int
ir_node_jcc_short_to_near(struct kedr_ir_node *node, 
	struct kedr_ifunc *func)
{
	u8 opcode = node->insn.opcode.bytes[0];
	u8 *pos = node->insn_buffer;
	unsigned int offset_opcode;
	static unsigned int len = 6; /* length of jcc near */
	
	/* The function may be called only for the nodes corresponding
	 * to the original instructions. */
	BUG_ON(node->orig_addr == 0);
	
	if (opcode < 0x70 || opcode > 0x7f)
		return 0;
	
	if (node->orig_addr + node->insn.length >= 
		(unsigned long)func->addr + func->size) {
		/* Weird. The conditional jump is at the end of the 
		 * function. It can be possible if the compiler expected the
		 * jump to always be performed, but still insisted on using 
		 * a conditional jump rather than jmp short for some reason.
		 * Or, more likely, someone meddled with label/symbol 
		 * declarations in the inline assembly parts (.global, 
		 * .local) and each part of the function looks like a 
		 * separate function as a result. 
		 * Anyway, warn and bail out, we cannot handle such split 
		 * functions. */
		pr_info("[sample] Warning: the conditional jump at %pS "
			"seems to be at the end of a function.\n",
			(void *)node->orig_addr);
		pr_info("[sample] Unable to perform instrumentation.\n");
		return -EILSEQ;
	}
	
	/* Leave the prefixes intact if any are present. */
	offset_opcode = insn_offset_opcode(&node->insn);
	pos += offset_opcode;
	
	/* Here we take advantage of the fact that the opcodes for short and
	 * near conditional jumps go in the same order with the last opcode 
	 * byte being 0x10 greater for jcc rel32, e.g.:
	 *   77 (ja rel8) => 0F 87 (ja rel32) 
	 *   78 (js rel8) => 0F 88 (js rel32), etc. */
	*pos++ = 0x0f;
	*pos++ = opcode + 0x10;
	
	*(u32 *)pos = X86_OFFSET_FROM_ADDR(node->orig_addr, 
		offset_opcode + len,
		node->dest_addr);
	
	/* Re-decode the instruction. */
	kernel_insn_init(&node->insn, node->insn_buffer);
	insn_get_length(&node->insn);
	
	BUG_ON(node->insn.length != offset_opcode + len);
	return 0;
}

/* If the instruction is jcxz or loop*, replace it with an equivalent 
 * sequence of instructions that uses jmp near to jump to the destination. 
 * The instruction in the node will be replaced with that near jump. 
 * For the other instructions of the sequence, new nodes will be created and
 * added before that 'reference' node. 
 * 
 * The function returns 0 on success or a negative error code on failure. 
 * The function does nothing if the node contains some other instruction. */
static int 
ir_node_jcxz_loop_to_jmp_near(struct kedr_ir_node *node, 
	struct kedr_ifunc *func)
{
	u8 opcode = node->insn.opcode.bytes[0];
	u8 *pos;
	struct kedr_ir_node *node_orig = NULL;
	struct kedr_ir_node *node_jump_over = NULL;
	
	/* The function may be called only for the nodes corresponding
	 * to the original instructions. */
	BUG_ON(node->orig_addr == 0);
	
	if (opcode < 0xe0 || opcode > 0xe3)
		return 0;
	/* loop/loope/loopne: 0xe0, 0xe1, 0xe2; jcxz: 0xe3. */
	
	if (node->orig_addr + node->insn.length >= 
		(unsigned long)func->addr + func->size) {
		/* Weird. The conditional jump is at the end of the 
		 * function. It can be possible if the compiler expected the
		 * jump to always be performed, but still insisted on using 
		 * a conditional jump rather than jmp short for some reason.
		 * Or, more likely, someone meddled with label/symbol 
		 * declarations in the inline assembly parts (.global, 
		 * .local) and each part of the function looks like a 
		 * separate function as a result. 
		 * Anyway, warn and bail out, we cannot handle such split 
		 * functions. */
		pr_info("[sample] Warning: the conditional jump at %pS "
			"seems to be at the end of a function.\n",
			(void *)node->orig_addr);
		pr_info("[sample] Unable to perform instrumentation.\n");
		return -EILSEQ;
	}
	
	/* j*cxz/loop* => 
	 *     <prefixes> j*cxz/loop* 02 (to label_jump, 
	 *                             length: 2 bytes + prefixes)
	 *     jmp short 05 (to label_continue, length: 2 bytes) 
	 * label_jump:
	 *     jmp near <where j*cxz would jump> (length: 5 bytes)
	 * label_continue:
	 *     ...  */
	node_orig = kedr_ir_node_create();
	node_jump_over = kedr_ir_node_create();
	if (node_orig == NULL || node_jump_over == NULL) {
		kedr_ir_node_destroy(node_orig);
		kedr_ir_node_destroy(node_jump_over);
		return -ENOMEM;
	}
	
	list_add(&node_orig->list, node->list.prev);
	list_add(&node_jump_over->list, &node_orig->list);
	node->first = node_orig;
	
	/* jcxz/loop* 02
	 * Copy the insn along with the prefixes it might have to the first
	 * node, set the jump offset properly. */
	memcpy(node_orig->insn_buffer, node->insn_buffer, 
		X86_MAX_INSN_SIZE);
	pos = node_orig->insn_buffer + insn_offset_immediate(&node->insn);
	*pos = 0x02;

	kernel_insn_init(&node_orig->insn, node_orig->insn_buffer);
	insn_get_length(&node_orig->insn);
	BUG_ON(node_orig->insn.length != 
		2 + insn_offset_opcode(&node->insn)); 
	/* +2: +1 for opcode, +1 for immediate */
	
	node_orig->dest_inner = node;
	
	/* jmp short 05 */
	pos = node_jump_over->insn_buffer;
	*pos++ = 0xeb;

	*pos = KEDR_SIZE_JMP_REL32; /* short jump over the near jump */
	node_jump_over->dest_inner = list_entry(node->list.next, 
		struct kedr_ir_node, list);

	kernel_insn_init(&node_jump_over->insn, node_jump_over->insn_buffer);
	insn_get_length(&node_jump_over->insn);
	BUG_ON(node_jump_over->insn.length != 2);

	/* Create the near jump to the destination in the reference node */
	pos = node->insn_buffer;
	*pos++ = KEDR_OP_JMP_REL32;
	*(u32 *)pos = X86_OFFSET_FROM_ADDR(node->orig_addr, 
		KEDR_SIZE_JMP_REL32,
		node->dest_addr);
	
	/* Re-decode the instruction. */
	kernel_insn_init(&node->insn, node->insn_buffer);
	insn_get_length(&node->insn);
	BUG_ON(node->insn.length != KEDR_SIZE_JMP_REL32);
	
	ir_node_update_block_start(node);
	return 0;
}

/* Replace short jumps (including jmp, jcc, jcxz, loop*) with the near 
 * relative jumps to the same destination. jcxz and loop* are replaced with 
 * the sequences of equivalent instructions that perform a near jump in the 
 * same conditions). 
 * 
 * The function returns 0 on success or a negative error code on failure. */
static int
ir_node_process_short_jumps(struct kedr_ir_node *node, 
	struct kedr_ifunc *func)
{
	int ret = 0;
	
	ir_node_jmp_short_to_near(node);
	
	ret = ir_node_jcc_short_to_near(node, func);
	if (ret != 0)
		return ret;
	
	ret = ir_node_jcxz_loop_to_jmp_near(node, func);
	if (ret != 0)
		return ret;
	
	/* If a formerly short jump lead outside of the function, set the 
	 * destination address as the address the resulting near jump
	 * jumps to. */
	if (node->insn.opcode.bytes[0] == KEDR_OP_JMP_REL32 &&
	    node->iprel_addr == 0) {
		BUG_ON(node->dest_addr == 0);
		BUG_ON(node->dest_addr == (unsigned long)(-1));
		if (!is_address_in_function(node->dest_addr, func))
			node->iprel_addr = node->dest_addr;
	}
	return 0;
}

/* A padding byte sequence is "00 00" (looks like "add al, (%rax)"). 
 * The instruction should be decoded before calling this function. */
static int
is_padding_insn(struct insn *insn)
{
	BUG_ON(insn->length == 0); 
	return (insn->opcode.value == 0 && insn->modrm.value == 0);
}

/* Checks if the function could be a part of a larger function but appear 
 * separate for some reason. 
 * is_incomplete_function() checks if the last meaningful instruction 
 * (non-noop and non-padding) is a control transfer instruction. If so, 
 * it returns 0, non-zero otherwise. 
 * 
 * Note that if is_incomplete_function() returns 0, it does not guarantee 
 * that the function is not incomplete. For example, it may have a jump at
 * the end that transfers control inside of another part of that larger 
 * function. For the present, we do not detect this. */
static int
is_incomplete_function(struct list_head *ir)
{
	struct kedr_ir_node *node;
	struct kedr_ir_node *last = NULL;
	
	list_for_each_entry(node, ir, list) {
		if (!is_padding_insn(&node->insn) && 
		    !insn_is_noop(&node->insn))
			last = node;
	}
	return (last == NULL || last->dest_addr == 0);
}
/* ====================================================================== */

int
instrument_function(struct kedr_ifunc *func, struct module *mod)
{
	int ret = 0;
	struct kedr_if_data if_data;
	struct kedr_ir_node *pos;
	struct kedr_ir_node *tmp;
	
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
	
	if (is_incomplete_function(&kedr_ir)) {
		pr_info("[sample] Warning: possibly incomplete function "
			"detected: \"%s\".\n",
			func->name);
		pr_info("[sample] Such functions may appear if there are "
	"'.global' or '.local' symbol definitions in the inline assembly "
	"within an original function.\n");
		pr_info("[sample] Or, may be, the function is written in "
	"some unusual way.\n");
		pr_info("[sample] Unable to perform instrumentation.\n");
		ret = -EILSEQ;
		goto out;
	}
	
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

	/* Split the code into blocks. */
	ir_mark_blocks(func, &kedr_ir);
	
	/* [NB] list_for_each_entry_safe() should also be safe w.r.t. the 
	 * addition of the nodes, not only against removal. 
	 * ir_node_process_short_jumps() may add new nodes before and after
	 *  '*pos' to do its work and these new nodes must not be traversed 
	 * in this loop. */
	list_for_each_entry_safe(pos, tmp, &kedr_ir, list) {
		ret = ir_node_process_short_jumps(pos, func);
		if (ret != 0)
			goto out;
	}
	
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
		debug_util_print_string("Code in IR:\n");
		list_for_each_entry(node, &kedr_ir, list) {
			if (node->block_starts)
				debug_util_print_string("[BS] ");
			debug_util_print_hex_bytes(node->insn_buffer, 
				node->insn.length);
			debug_util_print_string("\n");
		}
	}
	//<>

out:
	node_map_clear();
	ir_destroy(&kedr_ir);
	return ret;
}
/* ====================================================================== */
