/* instrument.c - instrumentation-related facilities. */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <kedr/asm/insn.h> /* instruction analysis support */

#include "instrument.h"
#include "util.h"
#include "detour_buffer.h"
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

/* The structure used to pass the required data to the instruction 
 * processing facilities (invoked by for_each_insn_in_function() in 
 * instrument_function() - hence "if_" in the name). 
 * The structure should be kept reasonably small in size so that it could be 
 * placed on the stack. */
struct kedr_if_data
{
	struct module *mod; /* target module */
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
	/*unsigned int i;*/
	struct kedr_jtable *jtable;
	/*int ret = 0;*/
	
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
	
	/* Local near indirect jumps may only jump to the beginning of a 
	 * block, so we need to add the contents of the jump table to the 
	 * array of block boundaries. */
	/*ret = block_offsets_reserve(if_data, num_elems);
	if (ret != 0)
		return ret;
	
	for (i = 0; i < num_elems; ++i) {
		unsigned long jaddr = *((unsigned long *)jtable_addr + i);
		if_data->block_offsets[if_data->num++] = 
			(u32)(jaddr - func_start);
	}*/
	
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

static int
do_process_insn(struct kedr_ifunc *func, struct insn *insn, void *data)
{
	int ret = 0;
	struct kedr_if_data *if_data = (struct kedr_if_data *)data;
	unsigned long start_addr;
	unsigned long offset_after_insn;
	u8 opcode;
	
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
	
	//<>
	// For now, just process indirect near jumps that can use jump 
	// tables. In the future - build the IR.
	opcode = insn->opcode.bytes[0];
	/* Some indirect near jumps need additional processing, namely those 
	 * that have the following form: 
	 * jmp near [<jump_table> + reg * <scale>]. 
	 * [NB] We don't need to do anything about other kinds of indirect 
	 * jumps, like jmp near [reg]. 
	 * 
	 * jmp near indirect has code FF/4. 'mod' and 'R/M' fields are used 
	 * here to determine if SIB byte is present. */
	if (opcode == 0xff && 
		X86_MODRM_REG(insn->modrm.value) == 4 && 
		X86_MODRM_MOD(insn->modrm.value) != 3 &&
		X86_MODRM_RM(insn->modrm.value) == 4) {
		ret = handle_jmp_near_indirect(func, insn, if_data);
		if (ret != 0)
			return ret;
	}
	//<>
	
	// TODO: record in IR which instruction each element in each
	// jump table refers to.
	
	// TODO
	return 0; 
}

/* Creates the jump tables for the instrumented instance of the function 
 * 'func' based on the jump tables for the original function. The jump 
 * tables will be filled with meaningful data during the instrumentation. 
 * For now, they will be just allocated, and the pointers to them will be 
 * stored in func->i_jump_tables[]. If an item of jump_table list has 0 
 * elements, the corresponding item in func->i_jump_tables[] will be NULL.
 *
 * [NB] The order of the corresponding indirect jumps and the order of the 
 * elements in func->jump_tables list must be the same. 
 *
 * [NB] In case of error, func->i_jump_tables will be freed in 
 * ifunc_destroy(), so it is not necessary to free it here. */
static int 
create_jump_tables(struct kedr_ifunc *func)
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
		}
		++i;
	}
	BUG_ON(i != func->num_jump_tables);
	return 0;
}
/* ====================================================================== */

/* Using the IR created before, perform the instrumentation. */
static int
do_instrument(struct kedr_ifunc *func)
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
	
	ret = skip_trailing_zeros(func);
	if (ret != 0)
		return ret;
	
	/* First, decode and process the machine instructions one by one and
	 * build the IR (TODO: IR). 
	 * 
	 * do_process_insn() will also adjust the length of the function if
	 * we have skipped too many zeros. */
	if_data.mod = mod;
	ret = for_each_insn_in_function(func, do_process_insn, 
		(void *)&if_data);
	if (ret != 0)
		return ret;
	
	/* Allocate and partially initialize the jump tables for the 
	 * instrumented instance. */
	ret = create_jump_tables(func);
	if (ret != 0)
		return ret;
	
	/* Create the instrumented instance of the function. */
	ret = do_instrument(func);
	if (ret != 0)
		return ret;
	
	// TODO: release memory occupied by the IR
	
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
	}
	//<>
	return 0;
}
/* ====================================================================== */

/* ====================================================================== */
