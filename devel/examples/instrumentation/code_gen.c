/* code_gen.c - generation of machine instructions needed for the
 * instrumentation. */
#include <linux/errno.h>
#include "code_gen.h"
/* ====================================================================== */

/* A special register code that means "no register". */
#define KEDR_REG_UNUSED (u8)(0xff)

/* Create Mod R/M byte from its parts.
 * For register codes, only the lower 3 bits are used. That is, the bit
 * provided by REX prefix (if any) is not written to Mod R/M byte. */
#define KEDR_MK_MODRM(_mod, _reg, _rm) \
	((_mod) << 6 | ((_reg) & 0x07) << 3 | ((_rm) & 0x07))

/* Create SIB byte from its parts. 
 * For register codes, only the lower 3 bits are used. That is, the bit 
 * provided by REX prefix (if any) is not written to SIB byte. */
#define KEDR_MK_SIB(_scale, _index, _base) \
	((_scale) << 6 | ((_index) & 0x07) << 3 | ((_base) & 0x07))
/* ====================================================================== */

/* Returns the node to operate on. It can be either a newly created node
 * added to the IR after 'base_node' or 'base_node' itself depending on 
 * 'in_place'. 
 * In case of an error, the return value is NULL, the error code is returned
 * in '*err'. */
static struct kedr_ir_node *
prepare_node(struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node = NULL;
	
	if (in_place)
		return base_node;
	
	node = kedr_ir_node_create();
	if (node == NULL) {
		*err = -ENOMEM;
		return NULL;
	}
	/* Insert the newly created node after '*base_node'. 
	 * Note that even if some kedr_mk_*() operation fails after that, 
	 * the resources used by the node will be reclaimed when the IR 
	 * is destroyed. */
	list_add(&node->list, &base_node->list);
			
	return node;
}

static void
decode_insn_in_node(struct kedr_ir_node *node)
{
	kernel_insn_init(&node->insn, &(node->insn_buffer[0]));
	insn_get_length(&node->insn);
	
	/* If the analyzer is not able to decode the instruction, we have
	 * probably written some garbage into 'node->insn_buffer' before. */
	BUG_ON(node->insn.length == 0);
}

/* write_rex_prefix() finds the appropriate REX prefix and writes it at 
 * 'write_at' if the prefix is necessary. The function returns the pointer 
 * to the byte after the prefix if the latter has been written, 'write_at'
 * otherwise. This allows to use the function in the statements like
 * 	p = write_rex_prefix(p, <...>)
 * 
 * 'full_size_default' - if non-zero, the instruction already operates on 
 * 	full-sized values by default. If 0, REX.W is necessary on x86-64 to 
 * 	make it do so.
 * 'r_reg' - code of the register specified by ModRM.Reg.
 * 'r_index' - code of the register specified by SIB.Index.
 * 'r_op_rm_base' - code of the register specified by a part of the opcode 
 * itself, by ModRM.RM or by SIB.Base.
 * 
 * See INAT_REG_CODE_<N> in inat.h for the list of register codes. For the 
 * registers (r_reg, r_index, r_op_rm_base) that are not used, 
 * KEDR_REG_UNUSED should be specified as the value. 
 * 
 * N.B. If ModRM.RM, SIB.Index or SIB.Base have special values that do not 
 * specify registers, KEDR_REG_UNUSED should also be used as the value of
 * the corresponding parameter. */
#ifdef CONFIG_X86_64
static u8 *
write_rex_prefix(u8 *write_at, int full_size_default, 
	u8 r_reg, u8 r_index, u8 r_op_rm_base)
{
	u8 rex = 0;
	if (!full_size_default)
		rex |= 0x48; /* 0100 1000: REX is needed; REX.W is set */
	
	if (r_reg != KEDR_REG_UNUSED && r_reg >= INAT_REG_CODE_8)
		rex |= 0x44; /* 0100 0100: REX is needed; REX.R is set */
	
	if (r_index != KEDR_REG_UNUSED && r_index >= INAT_REG_CODE_8)
		rex |= 0x42; /* 0100 0010: REX is needed; REX.X is set */
	
	if (r_op_rm_base != KEDR_REG_UNUSED && 
	    r_op_rm_base >= INAT_REG_CODE_8)
		rex |= 0x41; /* 0100 0001: REX is needed; REX.B is set */
	
	if (rex != 0)
		*write_at++ = rex;

	return write_at;
}

#else /* CONFIG_X86_32 */
static u8 *
write_rex_prefix(u8 *write_at, int full_size_default, 
	u8 r_reg, u8 r_index, u8 r_op_rm_base)
{
	/* No REX prefix on x86-32, do nothing. */
	return write_at;
}
#endif

/* write_modrm_expr() writes ModR/M, SIB (if necessary) and the displacement
 * to encode the expression <offset>(%base) at the specified position 
 * (pointed to by 'write_at'). Returns the pointer to the location 
 * immediately following the last byte it has written. 
 * 
 * The function takes into account that the base register ('r_base') can be
 * ESP/RSP or R12 and uses SIB form in such situations. 
 * If 'is_disp8' is nonzero, the lower 8 bits of offset will be used as the
 * displacement, the lower 32 bits otherwise. 
 * 'r_reg' is what should be written to Mod R/M byte as "reg" field. */
static u8 *
write_modrm_expr(u8 *write_at, u8 r_base, u8 r_reg, int is_disp8, 
	unsigned long offset)
{
	*write_at++ = KEDR_MK_MODRM((is_disp8 ? 1 : 2), r_reg, r_base);
	
	/* ESP/RSP or R12 as a base, use SIB == 0x24: 
	 * 00100100(b): 
	 * scale == 0; index == 100(b) - no index; base == 100(b). */
	if ((r_base & 0x07) == 4)
		*write_at++ = 0x24; 
	
	if (is_disp8) {
		*write_at++ = (u8)offset;
	}
	else {
		*(u32 *)write_at = (u32)offset;
		write_at += 4;
	}
	return write_at;
}
/* ====================================================================== */

/* mov %reg_from, %reg_to */
void
kedr_mk_mov_reg_to_reg(u8 reg_from, u8 reg_to, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	BUG_ON(reg_from >= X86_REG_COUNT);
	BUG_ON(reg_to >= X86_REG_COUNT);
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, reg_from, KEDR_REG_UNUSED, reg_to);
	
	*pos++ = 0x89; /* opcode */
	*pos++ = KEDR_MK_MODRM(3, reg_from, reg_to);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* Store mov %reg, <offset_reg>(%base)) or load 
 * (mov <offset_reg>(%base), %reg) depending on 'is_load'. 
 *
 * Here we make use of the fact that the array of spill slots for the 
 * registers is right at the beginning of the primary storage structure
 * %base points to. The number of the register is the number of its slot,
 * so <offset_regN> is sizeof(unsigned_long) * N. 1-byte displacement is
 * enough to encode such offsets. */
static void
mk_load_store_reg_slot(u8 reg, u8 base, int is_load, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	BUG_ON(reg >= X86_REG_COUNT);
	BUG_ON(base >= X86_REG_COUNT);
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, reg, KEDR_REG_UNUSED, base);
	
	*pos++ = is_load ? 0x8B : 0x89; /* opcode */
	pos = write_modrm_expr(pos, base, reg, 1, 
		(unsigned long)(reg * sizeof(unsigned long)));
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* mov %reg, <offset_reg>(%base) */
void
kedr_mk_store_reg_to_spill_slot(u8 reg, u8 base, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	mk_load_store_reg_slot(reg, base, 0, base_node, in_place, err);
}

/* mov <offset_reg>(%base), %reg */
void
kedr_mk_load_reg_from_spill_slot(u8 reg, u8 base, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	mk_load_store_reg_slot(reg, base, 1, base_node, in_place, err);
}

/* 'mov <expr>, %reg' or 'lea <expr>, %reg', depending on 'is_lea'.
 * <expr> is the addressing expression taken (constructed) from src->insn 
 * as is. */
static void
mk_mov_lea_expr_reg(struct insn *src, u8 reg, int is_lea,
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	unsigned long disp;

	/* The original instruction must have been decoded by now. */
	BUG_ON(src->length == 0); 
	/* The original instruction must have Mod R/M byte. */
	BUG_ON(src->modrm.nbytes != 1); 
	/* Mod R/M byte must define a memory addressing expression. */
	BUG_ON(X86_MODRM_MOD(src->modrm.value) == 3 /* 11(b) */ );
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	
#ifdef CONFIG_X86_64
	/* Construct a new REX prefix from the original one: take REX.X and
	 * REX.B as they are and set REX.W and REX.R appropriately. 
	 * If REX prefix was not present, it will be created anyway. */
	{
		u8 rex = (u8)src->rex_prefix.value; /* 0 if absent */
		rex |= 0x4C; /* 0100 1100, REX.W and REX.R are set */
		/* Unset REX.R if reg is one of the first 8 registers. */
		if (reg < INAT_REG_CODE_8)
			rex &= 0xFB; /* 1111 1011 */
		
		*pos++ = rex;
	}
#endif
	
	*pos++ = is_lea ? 0x8D : 0x8B;
	*pos++ = KEDR_MK_MODRM(
		X86_MODRM_MOD((u8)src->modrm.value),
		reg,
		X86_MODRM_RM((u8)src->modrm.value));
	
	if (src->sib.nbytes == 1)
		*pos++ = (u8)src->sib.value;
	
	disp = (unsigned long)src->displacement.value;

#ifdef CONFIG_X86_64
	/* If RIP-relative addressing is used, 'disp32' field should be set
	 * properly at the relocation phase. Here we just need to store the 
	 * destination address (the same as for 'src') in the node. */
	if (insn_rip_relative(src)) {
		disp = 0;
		node->iprel_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			src->kaddr, src->length, src->displacement.value);
	}
#endif
	 
	if (src->displacement.nbytes == 1) { /* disp8 */
		*pos++ = (u8)disp;
	} 
	else if (src->displacement.nbytes == 4) { /* disp32 */
		*(u32 *)pos = (u32)disp;
		pos += 4;
	}
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* lea <expr>, %reg */
void
kedr_mk_lea_expr_reg(struct insn *src, u8 reg,
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	mk_mov_lea_expr_reg(src, reg, 1, base_node, in_place, err);
}

/* mov <expr>, %reg */
void
kedr_mk_mov_expr_reg(struct insn *src, u8 reg,
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	mk_mov_lea_expr_reg(src, reg, 0, base_node, in_place, err);
}

/* push %reg */
void
kedr_mk_push_reg(u8 reg, struct kedr_ir_node *base_node, int in_place, 
	int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 1, KEDR_REG_UNUSED, KEDR_REG_UNUSED, 
		reg);
	
	*pos++ = 0x50 + (reg & 0x07);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* pop %reg */
void
kedr_mk_pop_reg(u8 reg, struct kedr_ir_node *base_node, int in_place, 
	int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 1, KEDR_REG_UNUSED, KEDR_REG_UNUSED, 
		reg);
	
	*pos++ = 0x58 + (reg & 0x07);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* call rel32 */
void
kedr_mk_call_rel32(unsigned long addr, struct kedr_ir_node *base_node, 
	int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	BUG_ON(addr == 0);
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = 0xe8;
	*(u32 *)pos = 0;
	pos += 4;
	
	/* The operand of this instruction will be set properly during the 
	 * relocation phase. For now, just save the destination address. */
	node->iprel_addr = addr;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* call *%reg */
void
kedr_mk_call_reg(u8 reg, struct kedr_ir_node *base_node, int in_place, 
	int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 1, KEDR_REG_UNUSED, KEDR_REG_UNUSED, 
		reg);
	/* Opcode: FF/2. ModRM.mod = 11(b) - reg only. */
	*pos++ = 0xff;
	*pos++ = KEDR_MK_MODRM(0x3, 0x2, reg);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* x86-64: sub <sign-extended lower 32 bits of 'value'>, %rax
 * x86-32: sub <value>, %eax */
void 
kedr_mk_sub_lower32b_from_ax(unsigned long value, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	u32 imm32 = (u32)value;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, 
		KEDR_REG_UNUSED, KEDR_REG_UNUSED, KEDR_REG_UNUSED);
	
	*pos++ = 0x2d;
	*(u32 *)pos = imm32;
	pos += 4;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* cmp <value32>, %rax */
void
kedr_mk_cmp_value32_with_ax(u32 value32, struct kedr_ir_node *base_node, 
	int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, 
		KEDR_REG_UNUSED, KEDR_REG_UNUSED, KEDR_REG_UNUSED);
	
	*pos++ = 0x3d;
	*(u32 *)pos = value32;
	pos += 4;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* jcc rel32 */
void
kedr_mk_jcc(u8 cc, struct kedr_ir_node *dest, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	BUG_ON(cc >= 0x10);
	BUG_ON(dest == NULL);
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = 0x0f;
	*pos++ = 0x80 + cc;
	*(u32 *)pos = 0; /* the offset does not really matter ... */
	pos += 4;
	
	/* ... but 'dest_inner' matters. */
	node->dest_inner = dest;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* ret near */
void
kedr_mk_ret(struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = 0xc3;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* xchg %rax, (%rsp) */
void
kedr_mk_xchg_ax_stack_top(struct kedr_ir_node *base_node, int in_place, 
	int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, 
		KEDR_REG_UNUSED, KEDR_REG_UNUSED, KEDR_REG_UNUSED);
	
	*pos++ = 0x87;
	*pos++ = 0x04;
	*pos++ = 0x24;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* x86-32: see b8 (Move imm32 to r32.)
 * x86-64: see c7 (Move imm32 sign extended to 64-bits to r/m64.) */
void
kedr_mk_mov_value32_to_ax(u32 value32, struct kedr_ir_node *base_node, 
	int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;

#ifdef CONFIG_X86_64
	*pos++ = 0x48; /* REX.W */
	*pos++ = 0xc7; /* C7/0: mov SignExt(imm32), %r/m64 */
	*pos++ = 0xc0; /* Mod R/M: mod==11(b) - reg, R/M == 0 - rax */
#else /* X86_32 */
	*pos++ = 0xb8; /* B8+r: mov imm32, %r */
#endif
	*(u32 *)pos = value32;
	pos += 4;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* mov value32, <offset>(%base) */
void
kedr_mk_mov_value32_to_slot(u32 value32, u8 base, u32 offset, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, KEDR_REG_UNUSED, KEDR_REG_UNUSED, 
		base);
	*pos++ = 0xc7;
	pos = write_modrm_expr(pos, base, 0, 0, (unsigned long)offset);
	*(u32 *)pos = value32;
	pos += 4;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* or value32, <offset>(%base) */
void
kedr_mk_or_value32_to_slot(u32 value32, u8 base, u32 offset, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, KEDR_REG_UNUSED, KEDR_REG_UNUSED, 
		base);
	*pos++ = 0x81; /* opcode: 81/1 */
	pos = write_modrm_expr(pos, base, 1, 0, (unsigned long)offset);
	*(u32 *)pos = value32;
	pos += 4;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* test %reg, %reg */
void 
kedr_mk_test_reg_reg(u8 reg, struct kedr_ir_node *base_node, int in_place,
	 int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, reg, KEDR_REG_UNUSED, reg);
	*pos++ = 0x85; /* 85/r */
	*pos++ = KEDR_MK_MODRM(3, reg, reg);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* jmp near <offset> - inner */
void 
kedr_mk_jmp_to_inner(struct kedr_ir_node *dest, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	BUG_ON(dest == NULL);
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = 0xe9;
	*(u32 *)pos = 0; /* the offset does not really matter ... */
	pos += 4;
	
	/* ... but 'dest_inner' matters. */
	node->dest_inner = dest;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* jmp near <offset> - to an external location */
void 
kedr_mk_jmp_to_external(unsigned long addr, struct kedr_ir_node *base_node, 
	int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	BUG_ON(addr == 0);
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = 0xe9;
	*(u32 *)pos = 0; 
	pos += 4;
	
	/* The operand of this instruction will be set properly during the 
	 * relocation phase. For now, just save the destination address. */
	node->iprel_addr = addr;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

#ifndef CONFIG_X86_64
/* mov %eax, <offset_reg_on_stack>(%esp) or 
 * xchg %eax, <offset_reg_on_stack>(%esp), depending on 'is_xchg'. */
void
kedr_mk_mov_eax_to_reg_on_stack(u8 reg, int is_xchg, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	u8 offset;
	
	if (*err != 0)
		return;
	
	BUG_ON(reg >= X86_REG_COUNT);
	offset = (u8)((7 - (int)reg) * (int)(sizeof (unsigned long)));
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = (is_xchg ? 0x87 : 0x89);
	*pos++ = 0x44; /* mod == 01(b) => disp8, reg == 000(b) => %eax, 
			* rm == 100(b) => SIB. */
	*pos++ = 0x24; /* no scale (00(b)), no index (100(b)), 
			* %esp as a base (100(b)). */
	*pos++ = offset;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}
#endif

/* jmp *<offset>(%base) */
void
kedr_mk_jmp_offset_base(u8 base, u32 offset, struct kedr_ir_node *base_node, 
	int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 1, KEDR_REG_UNUSED, KEDR_REG_UNUSED, 
		base);
	*pos++ = 0xff; /* Opcode: FF/4 */
	pos = write_modrm_expr(pos, base, 4, 0, (unsigned long)offset);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* xchg %reg1, %reg2 */
void
kedr_mk_xchg_reg_reg(u8 reg1, u8 reg2, struct kedr_ir_node *base_node, 
	int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, reg1, KEDR_REG_UNUSED, reg2);
	*pos++ = 0x87;
	*pos++ = KEDR_MK_MODRM(3, reg1, reg2); 
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* pushfq/pushfd */
void
kedr_mk_pushf(struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = 0x9c;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* popfq/popfd */
void
kedr_mk_popf(struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = 0x9d;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* sub %reg_what, %reg_from 
 * (%reg_from -= %reg_what) */
void
kedr_mk_sub_reg_reg(u8 reg_what, u8 reg_from, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, reg_what, KEDR_REG_UNUSED, 
		reg_from);
	*pos++ = 0x29;
	*pos++ = KEDR_MK_MODRM(3, reg_what, reg_from);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* add <value8>, %reg */
void
kedr_mk_add_value8_to_reg(u8 value8, u8 reg, 
	struct kedr_ir_node *base_node, int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, KEDR_REG_UNUSED, KEDR_REG_UNUSED,
		reg);
	*pos++ = 0x83; /* Opcode: 83/0 */
	*pos++ = KEDR_MK_MODRM(3, 0, reg);
	*pos++ = value8;
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* neg %reg */
void
kedr_mk_neg_reg(u8 reg, struct kedr_ir_node *base_node, int in_place, 
	int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, KEDR_REG_UNUSED, KEDR_REG_UNUSED,
		reg);
	*pos++ = 0xf7; /* Opcode: F7/3 */
	*pos++ = KEDR_MK_MODRM(3, 3, reg);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* add <offset_bx>(%base), %rax */
void
kedr_mk_add_slot_bx_to_ax(u8 base, struct kedr_ir_node *base_node, 
	int in_place, int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	BUG_ON(base >= X86_REG_COUNT);
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, INAT_REG_CODE_AX, KEDR_REG_UNUSED, 
		base);
	
	*pos++ = 0x03; /* opcode */
	pos = write_modrm_expr(pos, base, INAT_REG_CODE_AX, 1, 
		(unsigned long)(INAT_REG_CODE_BX * sizeof(unsigned long)));
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* add %rbx, %rax */
void
kedr_mk_add_bx_to_ax(struct kedr_ir_node *base_node, int in_place, 
	int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, INAT_REG_CODE_AX, KEDR_REG_UNUSED, 
		INAT_REG_CODE_BX);
	
	*pos++ = 0x03; /* opcode */
	*pos++ = KEDR_MK_MODRM(3, INAT_REG_CODE_AX, INAT_REG_CODE_BX);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* movzx %al, %rax (movzbq/movzbl %al, %rax/%eax) */
void
kedr_mk_movzx_al_ax(struct kedr_ir_node *base_node, int in_place, 
	int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	pos = write_rex_prefix(pos, 0, INAT_REG_CODE_AX, KEDR_REG_UNUSED, 
		INAT_REG_CODE_AX);
	
	*pos++ = 0x0f; /* opcode: 0F B6/r */
	*pos++ = 0xb6;
	*pos++ = KEDR_MK_MODRM(3, INAT_REG_CODE_AX, INAT_REG_CODE_AX);
	
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}

/* ud2 */
void
kedr_mk_ud2(struct kedr_ir_node *base_node, int in_place, 
	int *err)
{
	struct kedr_ir_node *node;
	u8 *pos;
	
	if (*err != 0)
		return;
	
	node = prepare_node(base_node, in_place, err);
	if (node == NULL)
		return;
	
	pos = node->insn_buffer;
	*pos++ = 0x0f; /* opcode: 0F 0B/r */
	*pos++ = 0x0b;
		
	decode_insn_in_node(node);
	BUG_ON(node->insn.length != 
		(unsigned char)(pos - node->insn_buffer));
}
/* ====================================================================== */
