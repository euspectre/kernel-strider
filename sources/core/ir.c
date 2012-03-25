/* ir.c - operations with the intermediate representation (IR) of the 
 * target's code. */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/hash.h>

#include <kedr/kedr_mem/block_info.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>

#include "config.h"
#include "core_impl.h"

#include "ir.h"
#include "util.h"
#include "i13n.h"
#include "module_ms_alloc.h"
#include "handlers.h"
#include "transform.h"
/* ====================================================================== */

/* Parameters of a hash map to be used to lookup IR nodes by the addresses 
 * of the corresponding machine instructions in the original code (see 
 * kedr_ir::node_map).
 * 
 * KEDR_IF_TABLE_SIZE - number of buckets in the table. */
#define KEDR_IF_HASH_BITS   10
#define KEDR_IF_TABLE_SIZE  (1 << KEDR_IF_HASH_BITS)
/* ====================================================================== */

/* Initialize the hash map of nodes. */
static void
node_map_init(struct hlist_head *node_map)
{
	unsigned int i = 0;
	for (; i < KEDR_IF_TABLE_SIZE; ++i)
		INIT_HLIST_HEAD(&node_map[i]);
}

/* Remove all the items from the node map. */
static void
node_map_clear(struct hlist_head *node_map)
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
node_map_add(struct kedr_ir_node *node, struct hlist_head *node_map)
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
node_map_lookup(unsigned long orig_addr, struct hlist_head *node_map)
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
 * of type O. 
 * The instruction must be decoded before it is passed to this function. */
static unsigned int
get_mem_size_type_o(struct kedr_ir_node *node)
{
	insn_attr_t *attr = &node->insn.attr;
	struct insn *insn = &node->insn;
	
	BUG_ON(insn->length == 0);
	
	if (attr->addr_method1 == INAT_AMETHOD_O) {
	    	return get_operand_size_from_insn_attr(insn, 
			attr->opnd_type1);
	}
	else if (attr->addr_method2 == INAT_AMETHOD_O) {
	    	return get_operand_size_from_insn_attr(insn, 
			attr->opnd_type2);
	}

	/* The function must be called only for the instructions of
	 * type O. */
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

static int
is_insn_type_x(struct insn *insn)
{
	insn_attr_t *attr = &insn->attr;
	return (attr->addr_method1 == INAT_AMETHOD_X ||
		attr->addr_method2 == INAT_AMETHOD_X);
}

static int
is_insn_type_y(struct insn *insn)
{
	insn_attr_t *attr = &insn->attr;
	return (attr->addr_method1 == INAT_AMETHOD_Y || 
		attr->addr_method2 == INAT_AMETHOD_Y);
}

static int
is_insn_type_xy(struct insn *insn)
{
	return (is_insn_type_x(insn) && is_insn_type_y(insn));
}

static int
is_insn_cmpxchg(struct insn *insn)
{
	u8 *opcode = insn->opcode.bytes;
	u8 modrm = insn->modrm.bytes[0];
	
	/* CMPXCHG: 0F B0 and 0F B1 */
	return (opcode[0] == 0x0f && 
		(opcode[1] == 0xb0 || opcode[1] == 0xb1) && 
		X86_MODRM_MOD(modrm) != 3);
}

static int
is_insn_cmpxchg8b_16b(struct insn *insn)
{
	u8 *opcode = insn->opcode.bytes;
	u8 modrm = insn->modrm.bytes[0];
	
	/* CMPXCHG8B/CMPXCHG16B: 0F C7 /1 */
	return (opcode[0] == 0x0f && opcode[1] == 0xc7 &&
		X86_MODRM_REG(modrm) == 1);
}

static int
is_insn_movbe(struct insn *insn)
{
	u8 *opcode = insn->opcode.bytes;
	
	/* We need to check the prefix to distinguish MOVBE from CRC32 insn,
	 * they have the same opcode. */
	if (insn_has_prefix(insn, 0xf2))
		return 0;
	
	/* MOVBE: 0F 38 F0 and 0F 38 F1 */
	return (opcode[0] == 0x0f && opcode[1] == 0x38 &&
		(opcode[2] == 0xf0 || opcode[2] == 0xf1));
}

// TODO: uncomment when implementing the instrumentation
//static int
//is_insn_setcc(struct insn *insn)
//{
//	u8 *opcode = insn->opcode.bytes;
//	u8 modrm = insn->modrm.bytes[0];
//	
//	/* SETcc: 0F 90 - 0F 9F */
//	return (opcode[0] == 0x0f && 
//		((opcode[1] & 0xf0) == 0x90) &&
//		X86_MODRM_MOD(modrm) != 3);
//}
//
//static int
//is_insn_cmovcc(struct insn *insn)
//{
//	u8 *opcode = insn->opcode.bytes;
//	u8 modrm = insn->modrm.bytes[0];
//	
//	/* CMOVcc: 0F 40 - 0F 4F */
//	return (opcode[0] == 0x0f && 
//		((opcode[1] & 0xf0) == 0x40) &&
//		X86_MODRM_MOD(modrm) != 3);
//}

/* Checks if the instruction has addressing method (type) E and its Mod R/M 
 * expression refers to memory.
 *
 * [NB] CMPXCHG, SETcc, etc. also have type E and will be reported by this 
 * function as such. To distinguish them from other type E instructions, use
 * is_*_cmpxchg() and the like. */
static int
is_insn_type_e(struct insn *insn)
{
	insn_attr_t *attr = &insn->attr;
	u8 modrm = insn->modrm.bytes[0];
	
	return ((attr->addr_method1 == INAT_AMETHOD_E || 
		attr->addr_method2 == INAT_AMETHOD_E) &&
		X86_MODRM_MOD(modrm) != 3);
}

static int
is_insn_xlat(struct insn *insn)
{
	u8 *opcode = insn->opcode.bytes;
	
	/* XLAT: D7 */
	return (opcode[0] == 0xd7);
}

static int
is_insn_direct_offset_mov(struct insn *insn)
{
	u8 *opcode = insn->opcode.bytes;
	
	/* Direct memory offset MOVs: A0-A3 */
	return (opcode[0] >= 0xa0 && opcode[0] <= 0xa3);
}

static int
is_insn_push_ev(struct insn *insn)
{
	u8 *opcode = insn->opcode.bytes;
	u8 modrm = insn->modrm.bytes[0];
	
	/* PUSH Ev: FF / 6 */
	return (opcode[0] == 0xff && X86_MODRM_REG(modrm) == 6);
}

static int
is_insn_pop_ev(struct insn *insn)
{
	u8 *opcode = insn->opcode.bytes;
	u8 modrm = insn->modrm.bytes[0];
	
	/* PUSH Ev: FF / 6 */
	return (opcode[0] == 0x8f && X86_MODRM_REG(modrm) == 0);
}

/* Returns nonzero if '*insn' is an I/O instruction that accesses memory,
 * namely INS or OUTS, 0 otherwise. */
static int
is_insn_io_mem_op(struct insn *insn)
{
	u8 *opcode = insn->opcode.bytes;
	
	/* INS: 6C or 6D; OUTS: 6E or 6F */
	return (opcode[0] >= 0x6c && opcode[0] <= 0x6f);
}

/* Checks if the instruction is a memory barrier but not a locked update, 
 * INS or OUTS. If so, the result will be nonzero and the type of the
 * barrier will be returned in '*bt'. Otherwise, 0 will be returned and 
 * '*bt' will remain unchanged. */
static int
is_insn_barrier_other(struct insn *insn, enum kedr_barrier_type *bt)
{
	u8 *opcode = insn->opcode.bytes;
	u8 modrm = insn->modrm.bytes[0]; /* 0 if no Mod R/M byte present */
	u8 mod = X86_MODRM_MOD(modrm);
	u8 reg = X86_MODRM_REG(modrm);
	
	/* *FENCE 
	 * [NB] The Intel's Manual (vol. 3B, Table A-6. Opcode Extensions 
	 * for One- and Two-byte Opcodes by Group Number) does not specify
	 * the value of ModRM.RM, while the AMD's manual ("General Purpose 
	 * and System Instructions") assumes ModRM.RM == 0 for these insns.
	 * I follow the Intel's manual here, just in case other values of 
	 * ModRM.RM are allowed for *FENCE. */
	if (opcode[0] == 0x0f && opcode[1] == 0xae && mod == 3 &&
	    reg >= 5 && reg <= 7) {
		if (reg == 5) /* 0F AE / 5 (mod = 11(b)), LFENCE */
			*bt = KEDR_BT_LOAD;
		else if (reg == 6) /* 0F AE / 6 (mod = 11(b)), MFENCE */
			*bt = KEDR_BT_FULL;
		else /* 0F AE / 7 (mod = 11(b)), SFENCE */
			*bt = KEDR_BT_STORE;
		
		return 1;
	}
	
	/* IN and OUT */
	if ((opcode[0] >= 0xe4 && opcode[0] <= 0xe7) || 
	    (opcode[0] >= 0xec && opcode[0] <= 0xef)) {
		*bt = KEDR_BT_FULL;
		return 1;
	}
	
	/* The serializing instructions processed below have 0x0F as the 
	 * first byte of their opcodes.
	 * [NB] Process the instructions with the first opcode byte 
	 * different from 0x0F before this statement (obvious, but still).*/
	if (opcode[0] != 0x0f)
		return 0;
	
	/* INVD, WBINVD */
	if (opcode[1] == 0x08 || opcode[1] == 0x09) {
		*bt = KEDR_BT_FULL;
		return 1;
	}
	
	/* INVLPG */
	if (opcode[1] == 0x01 && mod != 3 && reg == 7) {
		*bt = KEDR_BT_FULL;
		return 1;
	}
	
	/* CPUID */
	if (opcode[1] == 0xa2) {
		*bt = KEDR_BT_FULL;
		return 1;
	}
	
	/* MOV to CRn (except CR8 on x86-64).
	 * From Intel's Manual, vol. 3A, section 8.3 "Serializing 
	 * Instructions": 
	 * Privileged serializing instructions — <...> MOV (to control 
	 * register, with the exception of MOV CR8) <...>. 
	 * CR8 is only available in 64-bit mode. */
	if (opcode[1] == 0x22) {
#ifdef CONFIG_X86_64
		/* MOV r64 to CR8 */
		if (reg == 0 && X86_REX_R(insn->rex_prefix.bytes[0]))
			return 0;
#endif
		*bt = KEDR_BT_FULL;
		return 1;
	}
	
	/* MOV to DRn */
	if (opcode[1] == 0x23) {
		*bt = KEDR_BT_FULL;
		return 1;
	}
	
	/* [NB] There are other serializing instructions that act like 
	 * memory barriers but they seem unlikely to occur in the kernel 
	 * modules. If they do occur, they will be processed here. */
	return 0;
}

/* Opcode: FF/4 */
static int
is_insn_jump_near_indirect(struct insn *insn)
{
	return (insn->opcode.bytes[0] == 0xff && 
		X86_MODRM_REG(insn->modrm.bytes[0]) == 4);
}

/* JMP near relative (E9); Jcc near relative (0F 8x) */
static int
is_insn_jmp_jcc_rel32(struct insn *insn)
{
	u8 opcode = insn->opcode.bytes[0];
	return (opcode == 0xe9 || 
		(opcode == 0x0f && (insn->opcode.bytes[1] & 0xf0) == 0x80));
}

/* Opcode: FF/2 */
static int
is_insn_call_near_indirect(struct insn *insn)
{
	return (insn->opcode.bytes[0] == 0xff && 
		X86_MODRM_REG(insn->modrm.bytes[0]) == 2);
}

/* Opcode: E8 */
static int
is_insn_call_rel32(struct insn *insn)
{
	return (insn->opcode.bytes[0] == 0xe8);
}

/* Opcodes: FF/3 or 9A */
static int
is_insn_call_far(struct insn *insn)
{
	u8 opcode = insn->opcode.bytes[0];
	u8 modrm = insn->modrm.bytes[0];
	
	return (opcode == 0x9a || 
		(opcode == 0xff && X86_MODRM_REG(modrm) == 3));
}

/* Opcodes: FF/5 or EA */
static int
is_insn_jump_far(struct insn *insn)
{
	u8 opcode = insn->opcode.bytes[0];
	u8 modrm = insn->modrm.bytes[0];
	
	return (opcode == 0xea || 
		(opcode == 0xff && X86_MODRM_REG(modrm) == 5));
}

/* ====================================================================== */

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

struct kedr_ir_node *
kedr_ir_node_create(void)
{
	struct kedr_ir_node *node;
	
	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		return NULL;
	
	node->first = node;
	node->last  = node;
	
	node->reg_mask = X86_REG_MASK_ALL;
	node->cb_type = KEDR_CB_NONE;
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
/* ====================================================================== */

/* The structure used to pass the required data to the instruction 
 * processing facilities. 
 * The structure should be kept reasonably small in size so that it could be 
 * placed on the stack. */
struct kedr_ir_create_data
{
	struct module *mod;   /* target module */
	struct list_head *ir; /* intermediate representation of the code */
	struct hlist_head *node_map; /* map (original PC; node) */
};

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

/* Process the indirect near jump that may use a jump table: check if it 
 * does and if so, save information about this jump table in 'func'. */
static int 
process_jmp_near_indirect(struct kedr_ifunc *func, 
	struct kedr_ir_create_data *ic_data, struct kedr_ir_node *node)
{
	unsigned long jtable_addr;
	unsigned long end_addr = 0;
	int in_init = 0;
	int in_core = 0;
	struct module *mod = ic_data->mod;
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
		pr_warning(KEDR_MSG_PREFIX "Spurious jump table (?) at %p "
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
		if (!kedr_is_address_in_function(jaddr, func))
			break;
		++num_elems;
	}
	
	/* Store the information about this jump table in 'func'. It may be
	 * needed during instrumentation to properly fixup the contents of
	 * the table. */
	jtable = kzalloc(sizeof(*jtable), GFP_KERNEL);
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
	return 0;
}

/* See the description of kedr_ir_node::iprel_addr */
static int
ir_node_set_iprel_addr(struct kedr_ir_node *node, struct kedr_ifunc *func)
{
	u8 opcode = node->insn.opcode.bytes[0];
	if (opcode == KEDR_OP_CALL_REL32 || opcode == KEDR_OP_JMP_REL32) {
		BUG_ON(node->dest_addr == 0);
		BUG_ON(node->dest_addr == (unsigned long)(-1));
		
		if (!kedr_is_address_in_function(node->dest_addr, func))
			node->iprel_addr = node->dest_addr;
		return 0;
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

		if (kedr_is_address_in_function(node->iprel_addr, func)) {
			pr_warning(KEDR_MSG_PREFIX 
	"Warning: the instruction at %pS uses IP-relative addressing "
	"to access the code of the original function. "
	"Unable to instrument function %s().\n",
				(void *)node->orig_addr, func->name);
			return -EFAULT;
		}
	}
#endif
	/* node->iprel_addr remains 0 by default */
	return 0;
}

/* Check if the memory addressing expression uses %rsp/%esp. */
static int
expr_uses_sp(struct insn *insn)
{
	unsigned int expr_reg_mask = insn_reg_mask_for_expr(insn);
	return (expr_reg_mask & X86_REG_MASK(INAT_REG_CODE_SP));
} 

static int 
is_tracked_memory_op(struct insn *insn)
{
	/* Filter out indirect jumps and calls first, we do not track these
	 * memory accesses. */
	if (is_insn_call_near_indirect(insn) || 
	    is_insn_jump_near_indirect(insn) ||
	    is_insn_call_far(insn) || is_insn_jump_far(insn))
		return 0;
	
	if (insn_is_noop(insn))
		return 0;
	
	if (is_insn_type_e(insn) || is_insn_movbe(insn) || 
	    is_insn_cmpxchg8b_16b(insn)) {
	    	return (process_stack_accesses || !expr_uses_sp(insn));
	}
	
	if (is_insn_type_x(insn) || is_insn_type_y(insn))
		return 1;
	
	if (is_insn_direct_offset_mov(insn) || is_insn_xlat(insn))
		return 1;

	return 0;
}

/* Non-zero if the node corresponded to an instruction from the original
 * function when that node was created, that is, if it is a reference node.
 * 0 is returned otherwise. */
static int
is_reference_node(struct kedr_ir_node *node)
{
	return (node->orig_addr != 0);
}

static int
do_process_insn(struct kedr_ifunc *func, struct insn *insn, void *data)
{
	int ret = 0;
	struct kedr_ir_create_data *ic_data = 
		(struct kedr_ir_create_data *)data;
	u8 opcode;
	struct kedr_ir_node *node = NULL;
	
	BUG_ON(ic_data == NULL);
	
	/* [NB] We cannot skip the no-ops as they may be the destinations of
	 * the jumps. 
	 * For example, PAUSE instruction (F3 90) is a special kind of a nop
	 * that is used inside the spin-wait loops. Jumps to it are common. 
	 */
	
	/* Create and initialize the IR node and record the mapping 
	 * (address, node) in the hash map. */
	node = ir_node_create_from_insn(insn);
	if (node == NULL)
		return -ENOMEM;
	
	ret = ir_node_set_iprel_addr(node, func);
	if (ret != 0) {
		kedr_ir_node_destroy(node);
		return ret;
	}
	
	list_add_tail(&node->list, ic_data->ir);
	node_map_add(node, ic_data->node_map);
	
	/* Process indirect near jumps that can use jump tables, namely
	 * the jumps having the following form: 
	 * jmp near [<jump_table> + reg * <scale>]. 
	 * [NB] We don't need to do anything about other kinds of indirect 
	 * jumps, like jmp near [reg], here. 
	 * 
	 * jmp near indirect has opcode FF/4. Mod R/M and SIB fields are 
	 * used here to determine if this is the sort of jumps we need to 
	 * process. 
	 * Mod R/M == 0x24, SIB.Base == 5: 
	 *   reg == 100(b) - for FF/4; 
	 *   mod == 00(b), rm == 100(b), SIB.Base == 5 - SIB is present and
	 *   the addressing expression has the following form:
	 *   "<scaled_index> + disp32". */
	opcode = insn->opcode.bytes[0];
	if (opcode == 0xff && 
		insn->modrm.bytes[0] == 0x24 && 
		X86_SIB_BASE(insn->sib.value) == 5) {
		ret = process_jmp_near_indirect(func, ic_data, node);
		if (ret != 0)
			return ret;
	}
	
	/* Determine some of the properties of the instruction and set the 
	 * relevant flags in the node. These flags can be used when 
	 * calculating the number of the memory events to be reported in the
	 * block as well as the number of values in the local storage needed
	 * for these events. */
	if (is_tracked_memory_op(insn))
		node->is_tracked_mem_op = 1;
	
	if (is_insn_type_x(insn) || is_insn_type_y(insn))
		node->is_string_op = 1;
	
	if (is_insn_type_xy(insn))
		node->is_string_op_xy = 1;
	
	return 0; 
}

/* For each direct jump within the function, link its node in the IR to the 
 * node corresponding to the destination. */
static int
ir_make_links_for_jumps(struct kedr_ifunc *func, struct list_head *ir,
	struct hlist_head *node_map)
{
	struct kedr_ir_node *pos;

	WARN_ON(list_empty(ir));
	
	/* [NB] The address 0 is definitely outside of the function. */
	list_for_each_entry(pos, ir, list) {
		if (!kedr_is_address_in_function(pos->dest_addr, func))
			continue;
		pos->dest_inner = node_map_lookup(pos->dest_addr, node_map);
		
		/* If the jump destination is inside of this function, we 
		 * must have created the node for it and added this node to
		 * the hash map. */
		if (pos->dest_inner == NULL) {
			pr_warning(KEDR_MSG_PREFIX 
			"No IR element found for the instruction at %p\n", 
				(void *)pos->dest_addr);
			return -EFAULT;
		}
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
ir_prefill_jump_table(const struct kedr_jtable *jtable, 
	struct hlist_head *node_map)
{
	unsigned long *table = jtable->i_table;
	unsigned int i;
	for (i = 0; i < jtable->num; ++i) {
		struct kedr_ir_node *node = node_map_lookup(jtable->addr[i],
			node_map);
		table[i] = (unsigned long)node;
		if (table[i] == 0) {
			pr_warning(KEDR_MSG_PREFIX 
			"No IR element found for the instruction at %p\n", 
				(void *)(jtable->addr[i]));
			BUG();
		}
		
		/* The jump tables are prepared after the short jumps have
		 * been converted to the near jumps. So if the appropriate 
		 * item in a jump table refers to a node, it actually refers
		 * to 'node->first' and that node should be marked as the 
		 * start of a block. */
		node->first->block_starts = 1;
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
 * [NB] The jumps with "empty" jump tables will remain unchanged as we 
 * cannot predict where these jumps transfer control. We assume they lead
 * outside of the function (may not always be the case, but still). */
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
		 * the table must all be 1s, because the table resides in
		 * the module mapping space. */
		
		node->inner_jmp_indirect = 1;
		
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
 * [NB] In case of error, func->jt_buf will be freed when 'func' is 
 * destroyed, so it is not necessary to free it here. */
static int 
create_jump_tables(struct kedr_ifunc *func, struct list_head *ir,
	struct hlist_head *node_map)
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
	
	buf = kedr_module_alloc(total * sizeof(unsigned long));
	if (buf == NULL)
		return -ENOMEM;
	func->jt_buf = buf;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		if (jtable->num == 0) 
			continue;
		jtable->i_table = (unsigned long *)buf;
		buf = (void *)((unsigned long)buf + 
			jtable->num * sizeof(unsigned long));
		ir_prefill_jump_table(jtable, node_map);
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
		!kedr_is_address_in_function(node->dest_addr, func));
}

static int
is_jump_backwards(struct kedr_ir_node *node)
{
	return (node->dest_inner != NULL && 
		node->dest_inner->orig_addr <= node->orig_addr);
	/* "<=" is here rather than plain "<" just in case a jump to itself 
	 * is encountered. I have seen such jumps a couple of times in the 
	 * kernel modules, some special kind of padding, may be. */
}

/* Allocates an instance of 'kedr_call_info' for a given node, initializes
 * the fields which data are already known (depending on the type of the 
 * node), adds the instance to 'call_infos' in 'func'. The node must 
 * correspond to a near call/jump that transfers control to some other
 * function. */
static int
prepare_call_info(struct kedr_ir_node *node, struct kedr_ifunc *func)
{
	struct kedr_call_info *info;
	
	BUG_ON( node->cb_type != KEDR_CB_JUMP_INDIRECT_OUT &&
		node->cb_type != KEDR_CB_CALL_INDIRECT &&
		node->cb_type != KEDR_CB_CALL_REL32_OUT &&
		node->cb_type != KEDR_CB_JUMP_REL32_OUT);
	
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	
	info->pc = node->orig_addr;
	
	if (node->cb_type == KEDR_CB_CALL_REL32_OUT || 
	    node->cb_type == KEDR_CB_JUMP_REL32_OUT) {
		info->target = node->dest_addr;
		kedr_fill_call_info((unsigned long)info);
	}
	
	node->call_info = info;
	list_add(&info->list, &func->call_infos);
	return 0;
}

/* If the current instruction is a control transfer instruction, determine
 * whether it should be reflected in the set of code blocks (i.e. whether we
 * should mark some IR nodes as the beginnings of the blocks). 
 * 
 * If the current instruction is not a control transfer but still should
 * always be in a separate block, mark the block appropriately.
 *
 * [NB] Call this function only for the nodes already added to the IR 
 * because the information about the instruction following this one may be 
 * needed. 
 * 
 * [NB] Call this function after the jump tables have been analyzed (if 
 * there are any), that is, after create_jump_tables(), because that 
 * function marks the targets of the corresponding indirect jumps as the 
 * starts of the blocks and sets other relevant flags. */
static int
ir_node_set_block_starts(struct kedr_ir_node *node, struct list_head *ir, 
	struct kedr_ifunc *func)
{
	int ret = 0;
	
	/* The node should have been added to the IR before this function is 
	 * called. */
	BUG_ON(node->list.next == NULL);
	
	/* Locked update. */
	if (insn_is_locked_op(&node->insn)) {
		ir_mark_node_separate_block(node, ir);
		node->cb_type = KEDR_CB_LOCKED_UPDATE;
		node->barrier_type = KEDR_BT_FULL;
		return 0;
	}
	
	/* I/O operation accessing memory. */
	if (is_insn_io_mem_op(&node->insn)) {
		ir_mark_node_separate_block(node, ir);
		node->cb_type = KEDR_CB_IO_MEM_OP;
		node->barrier_type = KEDR_BT_FULL;
		return 0;
	}
	
	/* Some other kind of a memory barrier. */
	if (is_insn_barrier_other(&node->insn, &node->barrier_type)) {
		ir_mark_node_separate_block(node, ir);
		node->cb_type = KEDR_CB_BARRIER_OTHER;
		return 0;
	}
	
	/* Only control transfer instructions remain to be processed. */
	if (node->dest_addr == 0)
		return 0;
	
	/* Indirect near jump. */
	if (is_insn_jump_near_indirect(&node->insn)) {
		ir_mark_node_separate_block(node, ir);
		if (node->inner_jmp_indirect) {
			node->cb_type = KEDR_CB_JUMP_INDIRECT_INNER;
		}
		else {
			node->cb_type = KEDR_CB_JUMP_INDIRECT_OUT;
			ret = prepare_call_info(node, func);
			if (ret != 0)
				return ret;
		}
		return 0;
	}
	
	/* Indirect near call. */
	if (is_insn_call_near_indirect(&node->insn)) {
		ir_mark_node_separate_block(node, ir);
		node->cb_type = KEDR_CB_CALL_INDIRECT;
		ret = prepare_call_info(node, func);
		return ret;
	}
	
	/* JMP rel32, Jcc rel32 
	 * [NB] No need to process short jumps: they must have been 
	 * converted to the near jumps by now. */
	if (is_insn_jmp_jcc_rel32(&node->insn)) {
		if (is_transfer_outside(node, func)) {
			ir_mark_node_separate_block(node, ir);
			node->cb_type = KEDR_CB_JUMP_REL32_OUT;
			ret = prepare_call_info(node, func);
			if (ret != 0)
				return ret;
		}
		else if (is_jump_backwards(node)) {
			ir_mark_node_separate_block(node, ir);
			/* Transformation J* short => J* near might have 
			 * happened before, so 'dest_inner->first' should be
			 * referred to rather than 'dest_inner'. */
			node->dest_inner->first->block_starts = 1;
			node->cb_type = KEDR_CB_JUMP_BACKWARDS;
		}
		/* Other kinds of these jumps do not need to be placed in 
		 * separate blocks. */
		return 0;
	}
	
	/* CALL rel32 */
	if (is_insn_call_rel32(&node->insn)) {
		if (is_transfer_outside(node, func)) {
			ir_mark_node_separate_block(node, ir);
			node->cb_type = KEDR_CB_CALL_REL32_OUT;
			ret = prepare_call_info(node, func);
			if (ret != 0)
				return ret;
		}
		else if (is_jump_backwards(node)) {
			ir_mark_node_separate_block(node, ir);
			node->dest_inner->first->block_starts = 1;
			node->cb_type = KEDR_CB_JUMP_BACKWARDS;
		}
		/* Other kinds of these calls do not need to be placed in 
		 * separate blocks. */
		return 0;
	}
	 	
	/* Some other kind of control transfer: CALL/JMP far, RET, ... */
	ir_mark_node_separate_block(node, ir);
	node->cb_type = KEDR_CB_CONTROL_OUT_OTHER;
	return 0;
}

static struct kedr_block_info *
kedr_block_info_create(unsigned long max_events)
{
	struct kedr_block_info *bi = NULL;
	size_t s; 
	
	BUG_ON(max_events == 0);
	s = sizeof(struct kedr_block_info) + 
		(max_events - 1) * sizeof(struct kedr_mem_event);
	bi = kzalloc(s, GFP_KERNEL);
	if (bi == NULL)
		return NULL;

	bi->max_events = max_events;
	return bi;
}

/* If kedr_block_info instance is needed for the block starting with node
 * 'start', this function creates it and adds to the list in 'func'. 
 * 'max_events' is the maximum number of memory events in this block to be 
 * reported. 
 * 
 * Does nothing if 'start' is NULL. */
static int 
ir_create_block_info(struct kedr_ifunc *func, struct kedr_ir_node *start,
	unsigned long max_events)
{
	struct kedr_block_info *bi = NULL;
	
	if (start == NULL)
		return 0;
	BUG_ON(!start->block_starts || start->cb_type == KEDR_CB_NONE);
	
	if (max_events == 0)
		return 0;
	
	if (start->cb_type == KEDR_CB_LOCKED_UPDATE || 
	    start->cb_type == KEDR_CB_IO_MEM_OP) {
		BUG_ON(max_events != 1);
		bi = kedr_block_info_create(1);
		if (bi == NULL)
			return -ENOMEM;
		start->block_info = bi;
		list_add(&bi->list, &func->block_infos);
	}
	else if (start->cb_type == KEDR_CB_COMMON_NO_MEM_OPS) {
		/* This common block has memory events, adjust its type. */
		start->cb_type = KEDR_CB_COMMON;
		bi = kedr_block_info_create(max_events);
		if (bi == NULL)
			return -ENOMEM;
		start->block_info = bi;
		list_add(&bi->list, &func->block_infos);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
		"Unexpected: block of type %u at offset 0x%lx in %s() "
		"seems to contain tracked memory events.\n",
			(unsigned int)start->cb_type,
			start->orig_addr - (unsigned long)func->addr,
			func->name);
		BUG(); /* should not get here */
	}
	return 0;
}

/* A tracked memory operation that is not a string operation needs only
 * one local value. 
 * A string operation of type XY needs 4 values (2 events, address and 
 * size for each one), other string operations need 2 values each. 
 * Other operations need no values in the local storage, they are not 
 * tracked. */
static unsigned long
max_local_value_count(struct kedr_ir_node *node)
{
	if (!node->is_tracked_mem_op)
		return 0;
		
	if (!node->is_string_op)
		return 1;
	
	if (!node->is_string_op_xy)
		return 2;
	
	return 4;
}

static unsigned long
max_event_count(struct kedr_ir_node *node)
{
	if (!node->is_tracked_mem_op)
		return 0;
		
	if (!node->is_string_op_xy)
		return 1;

	return 2;
}

/* Mark the forward jumps leading out of the block (but still inside of the 
 * function) as such. The block starts with 'start' node.
 * This function may be called for KEDR_CB_COMMON blocks only. Because of 
 * that, we may assume that each jump in the block is a forward jump that
 * leads somewhere inside of the function. 
 * 
 * The function also saves the pointer to the last reference node of the 
 * block in 'start->end_node', it will be needed when instrumenting the 
 * jumps out of the block. */
static void
mark_jumps_out(struct kedr_ir_node *start, struct list_head *ir)
{
	struct kedr_ir_node *pos;
	struct kedr_ir_node *end_node;
	BUG_ON(!start->block_starts || start->cb_type != KEDR_CB_COMMON);
	
	/* Find the last reference node in the block. */
	pos = start;
	start->end_node = start;
	list_for_each_entry_from(pos, ir, list) {
		if (!is_reference_node(pos))
			continue;
		if (pos != start && pos->block_starts)
			break;
		start->end_node = pos;
	}
		
	/* Find the jumps out of the block and mark them. */
	pos = start;
	end_node = start->end_node;
	list_for_each_entry_from(pos, ir, list) {
		if (!is_reference_node(pos))
			continue;
		if (pos != start && pos->block_starts)
			break;
		if (pos->dest_inner != NULL && 
		    pos->dest_inner->orig_addr > end_node->orig_addr) {
		    	pos->jump_past_last = 1;
		    	start->block_has_jumps_out = 1;
		}
	}
}

static void
set_bit_in_mask(u8 num, u32 *mask)
{
	*mask |= 1 << num;
}


/* Set read and write masks according to the attributes of the insn. */
static void
set_masks_common(struct kedr_block_info *bi, struct kedr_ir_node *node, 
	unsigned long n)
{
	if (insn_is_mem_read(&node->insn))
		set_bit_in_mask(n, &bi->read_mask);
	
	if (insn_is_mem_write(&node->insn))
		set_bit_in_mask(n, &bi->write_mask);
}

static void
set_event_e_m_common(struct kedr_block_info *bi, 
	struct kedr_ir_node *node, unsigned long n)
{
	unsigned long sz = (unsigned long)get_mem_size_type_e_m(node);
	bi->events[n].pc = node->orig_addr;
	bi->events[n].size = sz;
}

/* fill_block_info_*() functions store the information about the instruction
 * in the given kedr_block_info instance and increase '*num' by the number 
 * of memory events to be reported for the instruction. */
static void
fill_block_info_xy(struct kedr_block_info *bi, struct kedr_ir_node *node, 
	unsigned long *num)
{
	unsigned long n = *num;
	unsigned long sz;
	
	set_bit_in_mask(n, &bi->string_mask);
	set_bit_in_mask(n + 1, &bi->string_mask);
	
	/* The first operation is always a read, the second one depends on
	 * the instruction (write for MOVS, read for CMPS). */
	set_bit_in_mask(n, &bi->read_mask);
	if (insn_is_mem_write(&node->insn))
		set_bit_in_mask(n + 1, &bi->write_mask);
	else
		set_bit_in_mask(n + 1, &bi->read_mask);
	
	sz = (unsigned long)get_mem_size_type_x_y(node);
	
	bi->events[n].pc = node->orig_addr;
	bi->events[n].size = sz;
	bi->events[n + 1].pc = node->orig_addr;
	bi->events[n + 1].size = sz;
	/* The size is not known yet, we only store the size of a single
	 * element now. */

	*num += 2;
}

static void
fill_block_info_x_or_y(struct kedr_block_info *bi, 
	struct kedr_ir_node *node, unsigned long *num)
{
	unsigned long n = *num;
	unsigned long sz;
	
	set_bit_in_mask(n, &bi->string_mask);
	
	set_masks_common(bi, node, n);
	sz = (unsigned long)get_mem_size_type_x_y(node);
	bi->events[n].pc = node->orig_addr;
	bi->events[n].size = sz;
	/* The size is not known yet, we only store the size of a single
	 * element now. */
	
	*num += 1;
}

static void
fill_block_info_doffset_mov(struct kedr_block_info *bi, 
	struct kedr_ir_node *node, unsigned long *num)
{
	unsigned long n = *num;
	
	set_masks_common(bi, node, n);
	bi->events[n].pc = node->orig_addr;
	bi->events[n].size = (unsigned long)get_mem_size_type_o(node);
	
	*num += 1;
}

static void
fill_block_info_xlat(struct kedr_block_info *bi, struct kedr_ir_node *node, 
	unsigned long *num)
{
	unsigned long n = *num;
	set_bit_in_mask(n, &bi->read_mask);
	bi->events[n].pc = node->orig_addr;
	bi->events[n].size = 1;

	*num += 1;
}

/* PUSH Ev and POP Ev are also type E instructions and the memory access via
 * their expression is tracked (if ModRM.mod != 11(b), as usual). We do not
 * track the access to the stack from these instructions, however. Although
 * these instructions both read and write from memory, we need to record 
 * only the type of access via their ModRM expressions. */
static void
fill_block_info_push_ev(struct kedr_block_info *bi, 
	struct kedr_ir_node *node, unsigned long *num)
{
	unsigned long n = *num;
	set_bit_in_mask(n, &bi->read_mask);
	set_event_e_m_common(bi, node, n);

	*num += 1;
}

static void
fill_block_info_pop_ev(struct kedr_block_info *bi, 
	struct kedr_ir_node *node, unsigned long *num)
{
	unsigned long n = *num;
	set_bit_in_mask(n, &bi->write_mask);
	set_event_e_m_common(bi, node, n);

	*num += 1;
}

static void
fill_block_info_cmpxchg(struct kedr_block_info *bi, 
	struct kedr_ir_node *node, unsigned long *num)
{
	unsigned long n = *num;
	/* Read happens always; it will be determined in runtime whether it 
	 * is an update. */
	set_bit_in_mask(n, &bi->read_mask);
	set_event_e_m_common(bi, node, n);

	*num += 1;
}

static void
fill_block_info_cmpxchg8b_16b(struct kedr_block_info *bi, 
	struct kedr_ir_node *node, unsigned long *num)
{
	unsigned long n = *num;
	u8 rex = node->insn.rex_prefix.bytes[0]; /* 0 on x86-32 */
	
	/* Read happens always; it will be determined in runtime whether it 
	 * is an update. */
	set_bit_in_mask(n, &bi->read_mask);
	bi->events[n].pc = node->orig_addr;
	bi->events[n].size = (X86_REX_W(rex) ? 16 : 8);

	*num += 1;
}

static void
fill_block_info_e_m_common(struct kedr_block_info *bi, 
	struct kedr_ir_node *node, unsigned long *num)
{
	unsigned long n = *num;
	
	set_masks_common(bi, node, n);
	set_event_e_m_common(bi, node, n);
	
	*num += 1;
}

/* Fill the masks and the event information in the kedr_block_info instance
 * for the block starting with 'start' if the appropriate data are already
 * known.
 * 'max_events' must have been already set. */
static void
fill_block_info(struct kedr_ir_node *start, struct list_head *ir)
{
	struct kedr_ir_node *pos = start;
	struct kedr_block_info *bi = start->block_info;
	unsigned long n = 0;
	
	list_for_each_entry_from(pos, ir, list) {
		if (pos != start && pos->block_starts)
			break;
		if (!pos->is_tracked_mem_op)
			continue;
		
		if (pos->is_string_op_xy) {
			fill_block_info_xy(bi, pos, &n);
			continue;
		}
		
		if (pos->is_string_op) { /* Type X or Y but not XY */
			fill_block_info_x_or_y(bi, pos, &n);
			continue;
		}
		
		if (is_insn_direct_offset_mov(&pos->insn)) {
			fill_block_info_doffset_mov(bi, pos, &n);
			continue;
		}
		
		if (is_insn_xlat(&pos->insn)) {
			fill_block_info_xlat(bi, pos, &n);
			continue;
		}
		
		if (is_insn_push_ev(&pos->insn)) {
			fill_block_info_push_ev(bi, pos, &n);
			continue;
		}
		
		if (is_insn_pop_ev(&pos->insn)) {
			fill_block_info_pop_ev(bi, pos, &n);
			continue;
		}
		
		if (is_insn_cmpxchg(&pos->insn)) {
			fill_block_info_cmpxchg(bi, pos, &n);
			continue;
		}
		
		if (is_insn_cmpxchg8b_16b(&pos->insn)) {
			fill_block_info_cmpxchg8b_16b(bi, pos, &n);
			continue;
		}
		
		BUG_ON(!is_insn_type_e(&pos->insn) && 
			!is_insn_movbe(&pos->insn));
		
		fill_block_info_e_m_common(bi, pos, &n);		
	}
	BUG_ON(n != bi->max_events);
}

/* Split the code into blocks, mark the starting nodes of the blocks as 
 * such, determine the types of the blocks, create kedr_block_info 
 * instances where needed and add them to the list in 'func'. */
static int
ir_create_blocks(struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_ir_node *pos;
	struct kedr_ir_node *start = NULL;
	int ret = 0;
	
	/* Max. number of memory events and of local values needed. */
	unsigned long max_events = 0; 
	unsigned long max_values = 0; 
	
	BUG_ON(list_empty(ir));
	pos = list_first_entry(ir, struct kedr_ir_node, list);
	pos->block_starts = 1;
	
	/* The first pass: process control transfer instructions and the 
	 * instructions that should always be in a separate block. */
	list_for_each_entry(pos, ir, list) {
		ret = ir_node_set_block_starts(pos, ir, func);
		if (ret != 0)
			return ret;
	}
	
	/* The second pass: determine the number of local values needed for 
	 * each common block, split the long blocks, adjust types, create
	 * kedr_block_info instances.
	 * 
	 * [NB] If creation of some kedr_block_info instance fails, we do 
	 * not need to destroy the previosly created ones here as they will
	 * be destroyed along with 'func' later. */
	list_for_each_entry(pos, ir, list) {
		unsigned long local_values = max_local_value_count(pos);
		
		if (!pos->block_starts && 
		    (max_values + local_values > KEDR_MAX_LOCAL_VALUES)) {
			pos->block_starts = 1;
		}

		if (pos->block_starts) {
			ret = ir_create_block_info(func, start, max_events);
			if (ret != 0)
				return ret;
			
			if (pos->cb_type == KEDR_CB_NONE)
				pos->cb_type = KEDR_CB_COMMON_NO_MEM_OPS;
			start = pos;
			max_events = 0;
			max_values = 0;
		}
		
		max_events += max_event_count(pos);
		max_values += local_values;
	}
	/* If the function was "incomplete", an error should have already 
	 * been reported and the execution would not get to 
	 * ir_create_blocks() at all. A function which is not incomplete 
	 * must end with some kind of a control transfer, possibly followed 
	 * by padding. Therefore, a block with tracked memory operations
	 * can not be the last one, so we do not need to handle this case
	 * here. */
	
	/* The third pass: 
	 * - mark jumps out of the blocks (KEDR_CB_COMMON only);
	 * - fill the remaining fields of kedr_block_info instances (masks,
	 * event information). */
	list_for_each_entry(pos, ir, list) {
		if (!pos->block_starts)
			continue;
		if (pos->cb_type == KEDR_CB_COMMON)
			mark_jumps_out(pos, ir);
		if (pos->block_info != NULL)
			fill_block_info(pos, ir);
	}
	return 0;
}

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
		pr_warning(KEDR_MSG_PREFIX 
			"Warning: the conditional jump at %pS "
			"seems to be at the end of a function.\n",
			(void *)node->orig_addr);
		pr_warning(KEDR_MSG_PREFIX 
			"Unable to perform instrumentation.\n");
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
	BUG_ON(!is_reference_node(node));
	
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
		pr_warning(KEDR_MSG_PREFIX 
			"Warning: the conditional jump at %pS "
			"seems to be at the end of a function.\n",
			(void *)node->orig_addr);
		pr_warning(KEDR_MSG_PREFIX 
			"Unable to perform instrumentation.\n");
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
	/* Do not set dest_inner here, do that only for jmp/jcc nodes. */
	
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
		if (!kedr_is_address_in_function(node->dest_addr, func))
			node->iprel_addr = node->dest_addr;
	}
	return 0;
}

int 
kedr_ir_create(struct kedr_ifunc *func, struct kedr_i13n *i13n, 
	struct list_head *ir)
{
	int ret = 0;
	struct hlist_head *node_map = NULL;
	struct kedr_ir_create_data ic_data;
	struct kedr_ir_node *pos;
	struct kedr_ir_node *tmp;
	
	BUILD_BUG_ON(KEDR_MAX_LOCAL_VALUES > sizeof(unsigned long) * 8);
	BUG_ON(ir == NULL);
	
	/* Create the hash map (original address; IR node). */
	node_map = kzalloc(sizeof(struct hlist_head) * KEDR_IF_TABLE_SIZE, 
		GFP_KERNEL);
	if (node_map == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"kedr_ir_create(): not enough memory.\n");
		return -ENOMEM;
	}
	node_map_init(node_map);
	
	/* First, decode and process the machine instructions one by one and
	 * build the IR, without inter-node links at this stage. In 
	 * addition, the mapping table (node_map) will be filled there. */
	ic_data.mod = i13n->target;
	ic_data.ir = ir;
	ic_data.node_map = node_map;
	ret = kedr_for_each_insn_in_function(func, do_process_insn, 
		(void *)&ic_data);
	if (ret != 0)
		goto out;
	
	if (is_incomplete_function(ir)) {
		pr_warning(KEDR_MSG_PREFIX 
		"Warning: possibly incomplete function detected: \"%s\".\n",
			func->name);
		pr_warning(KEDR_MSG_PREFIX 
		"Such functions may appear if there are '.global' or "
		"'.local' symbol definitions in the inline assembly "
		"within an original function.\n");
		pr_warning(KEDR_MSG_PREFIX 
		"Or, may be, the function is written in an unusual way.\n");
		
		pr_warning(KEDR_MSG_PREFIX 
			"Unable to perform instrumentation.\n");
		ret = -EILSEQ;
		goto out;
	}
	
	ret = ir_make_links_for_jumps(func, ir, node_map);
	if (ret != 0)
		goto out;
	
	/* [NB] list_for_each_entry_safe() should also be safe w.r.t. the 
	 * addition of the nodes, not only against removal. 
	 * ir_node_process_short_jumps() may add new nodes before and after
	 * '*pos' to do its work and these new nodes must not be traversed 
	 * in this loop. */
	list_for_each_entry_safe(pos, tmp, ir, list) {
		ret = ir_node_process_short_jumps(pos, func);
		if (ret != 0)
			goto out;
	}
	
	/* Allocate and partially initialize the jump tables for the 
	 * instrumented instance. 
	 * At this stage, the jump tables will be filled with pointers
	 * to the corresponding IR nodes rather than the instructions 
	 * themselves. */
	ret = create_jump_tables(func, ir, node_map);
	if (ret != 0)
		goto out;
	
	ret = ir_create_blocks(func, ir);
	if (ret != 0)
		goto out;
		
	node_map_clear(node_map);
	kfree(node_map);
	return 0;
out:
	node_map_clear(node_map);
	kfree(node_map);
	kedr_ir_destroy(ir);
	return ret;
}

void
kedr_ir_destroy(struct list_head *ir)
{
	struct kedr_ir_node *pos;
	struct kedr_ir_node *tmp;
	
	BUG_ON(ir == NULL);
	
	list_for_each_entry_safe(pos, tmp, ir, list) {
		list_del(&pos->list);
		kedr_ir_node_destroy(pos);
	}
}
/* ====================================================================== */

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
	
	/* Handle 'ret' group to avoid marking scratch registers as used for 
	 * these instructions. Same for 'iret'. */
	if (opcode == 0xc3 || opcode == 0xc2 || 
	    opcode == 0xca || opcode == 0xcb ||
	    opcode == 0xcf)
		return X86_REG_MASK(INAT_REG_CODE_SP);
	
	reg_mask = insn_reg_mask(insn);
	dest = insn_jumps_to(insn);
	
	if (dest != 0 && 
	    (dest < start_addr || dest >= start_addr + func->size))
	    	reg_mask |= X86_REG_MASK_SCRATCH;
		
	return reg_mask;
}

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
			pr_warning(KEDR_MSG_PREFIX 
	"The instruction at %pS seems to use all general-purpose registers "
	"and is neither PUSHAD nor POPAD. Unable to instrument function "
	"%s().\n",
				(void *)node->orig_addr, func->name);
			return -EINVAL;
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

	return base;
}

/* Returns non-zero if the instruction is a "simple" exit from the function
 * (that is, an exit that required only the standard processing):
 * RET*, IRET, UD2, JMP far. 
 * Note that near jumps that can also be function exits do not fall into 
 * this group as they require special processing. */
static int 
is_simple_function_exit(struct insn *insn)
{
	u8 opcode = insn->opcode.bytes[0];
	u8 modrm = insn->modrm.bytes[0];
	
	/* RET*, IRET */
	if (opcode == 0xc2 || opcode == 0xc3 || opcode == 0xca || 
	    opcode == 0xcb || opcode == 0xcf)
		return 1;
	
	/* UD2 */
	if (opcode == 0x0f && insn->opcode.bytes[1] == 0x0b)
		return 1;
	
	/* JMP far */
	if (opcode == 0xea || (opcode == 0xff && X86_MODRM_REG(modrm) == 5))
		return 1;
	
	return 0;
}

int 
kedr_ir_instrument(struct kedr_ifunc *func, struct list_head *ir)
{
	int ret;
	u8 base;
	struct kedr_ir_node *node;
	struct kedr_ir_node *tmp;
	
	BUG_ON(ir == NULL);
	
	ret = ir_choose_base_register(func, ir);
	if (ret < 0)
		return ret;
	base = (u8)ret;
	
	/* Phase 1:
	 * - handle the instructions that use the base register and 
	 *   "release" it;
	 * - handle function entry and exits; 
	 * - handle function calls;
	 * - handle inner indirect near jumps that use the base register. */
	ret = kedr_handle_function_entry(ir, func, base);
	if (ret < 0)
		return ret;
	
	list_for_each_entry_safe(node, tmp, ir, list) {
		if (!is_reference_node(node))
			continue;
		
		ret = 0;
		if (is_simple_function_exit(&node->insn))
			ret = kedr_handle_function_exit(node, base);
		else if (node->cb_type == KEDR_CB_CALL_INDIRECT)
			ret = kedr_handle_call_indirect(node, base);
		else if (node->cb_type == KEDR_CB_JUMP_INDIRECT_OUT)
			ret = kedr_handle_jmp_indirect_out(node, base);
		else if (node->cb_type == KEDR_CB_JUMP_INDIRECT_INNER)
			ret = kedr_handle_jmp_indirect_inner(node, base);
		else if (node->cb_type == KEDR_CB_CALL_REL32_OUT)
			ret = kedr_handle_call_rel32_out(node, base);
		else if (node->cb_type == KEDR_CB_JUMP_REL32_OUT)
			ret = kedr_handle_jxx_rel32_out(node, base);
		else if (is_pushad(&node->insn))
			ret = kedr_handle_pushad(node, base);
		else if (is_popad(&node->insn))
			ret = kedr_handle_popad(node, base);
		else 
			/* General case, just "release" %base register. */
			ret = kedr_handle_general_case(node, base);
		
		if (ret < 0)
			return ret;
	}
	
	// TODO: Phase 2
	// TODO: Do not forget to process barriers
	return 0;
}
/* ====================================================================== */

/* Updates the offsets of the instructions from the beginning of the 
 * instrumented instance.
 * The function returns non-zero if the new offset is different from the
 * old one for at least one node, 0 otherwise. */
static int
ir_update_offsets(struct list_head *ir)
{
	struct kedr_ir_node *node;
	int changed = 0;
	long offset = 0;
	
	list_for_each_entry(node, ir, list) {
		changed |= (node->offset != offset);
		node->offset = offset;
		offset += (long)node->insn.length;
	}
	return changed;
}

/* For the nodes with non-NULL 'dest_inner', update that field to actually
 * point to the destination node. */
static void 
ir_resolve_dest_inner(struct list_head *ir)
{
	struct kedr_ir_node *node;
	struct kedr_ir_node *dest;
	
	list_for_each_entry(node, ir, list) {
		dest = node->dest_inner;
		if (dest == NULL)
			continue; 
		
		if (node->jump_past_last) {
			//<> TODO: Uncomment when the instrumentation of
			// the ends of the common blocks is implemented.
			// Remove "dest = dest->first;" statement.
			// This is a temporary "short-circuit" change to 
			// be able to test detoured execution before the
			// rest of the core is prepared.
			dest = dest->first;
			/*
			BUG_ON(dest->last->list.next == ir);
			dest = list_entry(dest->last->list.next,
				struct kedr_ir_node, list);
			*/
			//<>
		}
		else {
			dest = dest->first;
		}
		node->dest_inner = dest; 
	}
}

/* Process direct inner jumps: choose among their near and short versions.
 * If the instruction is changed by this function, it will be re-decoded.
 * By the time this function is called, 'dest_inner' should already point 
 * to the actual destination nodes of these jumps rather than to the 
 * corresponding reference nodes. */
static void
ir_set_inner_jump_length(struct list_head *ir)
{
	struct kedr_ir_node *node;
	long disp;
	unsigned int prefix_len;
	u8 *pos;
	u8 opcode;
	
	list_for_each_entry(node, ir, list) {
		if (node->dest_inner == NULL)
			continue; 
		
		/* Assume the jump is short, so the size of the instruction
		 * is <size_of_prefixes> + 2. Calculate the displacement
		 * to the destination (NB: it is not final yet!). */
		prefix_len = insn_offset_opcode(&node->insn);
		disp = node->dest_inner->offset - (node->offset + 
			(long)prefix_len + 2);
		if (disp > 127 || disp < -128)
			continue; /* too long distance, leave as is */
		
		/* Make the jump short. The displacement in the instruction
		 * will be set later. Set 0 for now - just in case. */
		opcode = node->insn.opcode.bytes[0];
		pos = node->insn_buffer + prefix_len;
		if (opcode == 0xe9) { /* jmp near => jmp short */
			*pos++ = 0xeb;
			*pos++ = 0;
			kernel_insn_init(&node->insn, node->insn_buffer);
			insn_get_length(&node->insn);
			BUG_ON(node->insn.length != 
				(unsigned char)(pos - node->insn_buffer));
		}
		else if (opcode == 0x0f && 
			(node->insn.opcode.bytes[1] & 0xf0) == 0x80) {
			/* jcc near => jcc short */
			*pos++ = node->insn.opcode.bytes[1] - 0x10;
			*pos++ = 0;
			kernel_insn_init(&node->insn, node->insn_buffer);
			insn_get_length(&node->insn);
			BUG_ON(node->insn.length != 
				(unsigned char)(pos - node->insn_buffer));
		}
		/* Neither jmp near nor jcc near? Do nothing then, it might
		 * be a short jump already, or a mov generated when handling
		 * a jump out of the block, or something like 'call $+5', 
		 * etc. */
	}
}

/* Sets the displacements in jmp/jcc, short and near. By the time this 
 * function is called, 'dest_inner' should already point to the actual 
 * destination nodes of these jumps rather than to the corresponding 
 * reference nodes. 
 * [NB] This function does not re-decode the instructions it changes. This
 * should not be a problem because all the fields except 'immediate' will
 * remain valid. */
static void
ir_set_inner_jump_disp(struct list_head *ir)
{
	struct kedr_ir_node *node;
	u8 opcode;
	u8 *pos;
	long disp;
	
	list_for_each_entry(node, ir, list) {
		if (node->dest_inner == NULL)
			continue;
		
		disp = node->dest_inner->offset - (node->offset + 
			(long)node->insn.length);
		pos = node->insn_buffer + insn_offset_immediate(&node->insn);
				
		opcode = node->insn.opcode.bytes[0];
		if (opcode == 0xeb || (opcode & 0xf0) == 0x70) {
			/* jmp/jcc short */
			BUG_ON(disp < -128 || disp > 127);
			*pos = (u8)disp;
		}
		else { /* jmp/jcc near, mov (handling of jumps out of the 
			block) or something like call $+5; imm32 assumed. */
			*(u32 *)pos = (u32)disp;
		}
		
	}
}

/* Replace the addresses of the nodes in the jump tables with the offsets 
 * of the instructions the jumps should lead to. Note that each jump 
 * destination is not always the node itself but 'node->first'. */
static void
fill_jump_tables(struct kedr_ifunc *func)
{
	struct kedr_jtable *jtable;
	unsigned long *table;
	struct kedr_ir_node *node;
	unsigned int i;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		if (jtable->num == 0)
			continue;
		table = jtable->i_table;
		for (i = 0; i < jtable->num; ++i) {
			node = (struct kedr_ir_node *)table[i];
			table[i] = (unsigned long)node->first->offset;
		}
	}
}

/* If a relocation should be created for the given node, this function
 * does so and adds the relocation to the list of relocations in 'func'. 
 * The type of the relocation is inferred from the node. */
static int 
add_relocation(struct kedr_ifunc *func, struct kedr_ir_node *node)
{
	struct kedr_reloc *reloc = NULL;
	
	if (node->iprel_addr == 0 && !node->needs_addr32_reloc)
		return 0; /* nothing to do */
		
	reloc = kzalloc(sizeof(*reloc), GFP_KERNEL);
	if (reloc == NULL)
		return -ENOMEM;
	
	reloc->offset = node->offset;
	if (node->iprel_addr != 0) {
		reloc->rtype = KEDR_RELOC_IPREL;
		reloc->dest = (void *)node->iprel_addr;
	}
	else /* node->needs_addr32_reloc */ {
		reloc->rtype = KEDR_RELOC_ADDR32;
	}
	
	list_add_tail(&reloc->list, &func->relocs);
	return 0;
}

/* Creates the temporary buffer and places the instrumented code there.
 * During that process, relocation records are created for the nodes with 
 * iprel_addr != 0 to func->relocs as well as for those with 
 * needs_addr32_reloc. 
 * The function sets 'func->i_size'. */
static int
generate_code(struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_ir_node *node;
	size_t size_of_code;
	u8 *buf;
	int ret = 0;
	
	BUG_ON(list_empty(ir));
	
	/* Determine the size of the code: the offset of the last 
	 * instruction + the length of that instruction. */
	node = list_entry(ir->prev, struct kedr_ir_node, list);
	size_of_code = (size_t)node->offset + (size_t)node->insn.length;
	BUG_ON(size_of_code == 0);
	
	/* [NB] If an error occurs, we need not bother and delete relocation
	 * records created so far as well as 'func->tbuf'. All these data 
	 * will be deleted at once when 'func' is deleted. */
		
	func->tbuf = kzalloc(size_of_code, GFP_KERNEL);
	if (func->tbuf == NULL)
		return -ENOMEM;
	
	buf = (u8 *)func->tbuf;
	list_for_each_entry(node, ir, list) {
		size_t len = node->insn.length;
		memcpy(buf, &node->insn_buffer[0], len);
		
		ret = add_relocation(func, node);
		if (ret < 0)
			return ret;
		
		buf += len;
	}
	
	func->i_size = (unsigned long)size_of_code;
	return 0;
}

int 
kedr_ir_generate_code(struct kedr_ifunc *func, struct list_head *ir)
{
	int ret = 0;
	int offsets_changed = 0;
	
	BUG_ON(ir == NULL);
	BUG_ON(func->tbuf != NULL);
	BUG_ON(!list_empty(&func->jump_tables) && func->jt_buf == NULL);
	
	/* Choose the length of the inner jumps and set the final offsets
	 * of the instructions. */
	ir_resolve_dest_inner(ir);
	ir_update_offsets(ir);
	do {
		ir_set_inner_jump_length(ir);
		offsets_changed = ir_update_offsets(ir);
	}
	while (offsets_changed);
	ir_set_inner_jump_disp(ir);
	
	/* Replace the pointers in the jump tables with the offsets of the 
	 * destination instructions. */
	fill_jump_tables(func);
	
	ret = generate_code(func, ir);
	return ret;
}
/* ====================================================================== */
