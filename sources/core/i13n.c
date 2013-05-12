/* i13n.c - the top-level component of the instrumentation subsystem. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/hash.h>
#include <linux/spinlock.h>

#include <kedr/kedr_mem/functions.h>

#include "config.h"
#include "core_impl.h"

#include "sections.h"
#include "module_ms_alloc.h"
#include "i13n.h"
#include "ifunc.h"
#include "ir.h"
#include "util.h"
#include "hooks.h"
#include "fh_impl.h"
/* ====================================================================== */

static void
free_fallback_areas(struct kedr_i13n *i13n)
{
	/* kedr_module_free(NULL) is a no-op anyway */
	kedr_module_free(i13n->fallback_init_area);
	i13n->fallback_init_area = NULL;
	kedr_module_free(i13n->fallback_core_area);
	i13n->fallback_core_area = NULL;
}

static int
alloc_fallback_areas(struct kedr_i13n *i13n)
{
	struct module *mod = i13n->target;
	/* Here we copy the code of the target module to some areas in the
	 * module mapping space. The functions contained there will be fixed
	 * up later and will serve as fallback functions in case something
	 * bad is detected by the instrumented code in runtime. For example,
	 * If the function call allocating the local storage fails, it is
	 * not an option to let the instrumented function continue. Calling
	 * BUG() is not quite user-friendly. So, in such situations, control
	 * will be transferred to a fallback instance of the original 
	 * function and it should execute as usual. 
	 * The original function itself will be modified, a jump to the 
	 * instrumented code will be placed at its beginning, so we cannot 
	 * let the control to pass to it. That's why we need these fallback
	 * instances.
	 * Note that after module loading notifications are handled, the
	 * module loader may make the code of the module read only, so we 
	 * cannot uninstrument it and pass control there in runtime either.
	 */
	if (kedr_has_init_text(mod)) {
		i13n->fallback_init_area = kedr_module_alloc(
			mod->init_text_size);
		if (i13n->fallback_init_area == NULL)
			goto no_mem;
		
		memcpy(i13n->fallback_init_area, mod->module_init, 
			mod->init_text_size);
	}
	
	if (kedr_has_core_text(mod)) {
		i13n->fallback_core_area = kedr_module_alloc(
			mod->core_text_size);
		if (i13n->fallback_core_area == NULL)
			goto no_mem;
		
		memcpy(i13n->fallback_core_area, mod->module_core,
			mod->core_text_size);
	}
	return 0; /* success */

no_mem:
	free_fallback_areas(i13n);	
	return -ENOMEM;
}
/* ====================================================================== */

/* Parameters of the hash table used to lookup func_info objects by the 
 * addresses of the original functions in the target module.
 * 
 * KEDR_FUNC_INFO_TABLE_SIZE - number of buckets in the table. */
#define KEDR_FUNC_INFO_HASH_BITS   16
#define KEDR_FUNC_INFO_TABLE_SIZE  (1 << KEDR_FUNC_INFO_HASH_BITS)

struct kedr_fi_table_item
{
	struct hlist_node hlist;
	struct kedr_func_info *fi;
};

/* Creates the hash table. */
static int 
create_fi_table(struct kedr_i13n *i13n)
{
	int i;
	
	/* i13n->fi_table is NULL because '*i13n' has been allocated with 
	 * kzalloc(). */
	i13n->fi_table = vmalloc(
		sizeof(struct hlist_head) * KEDR_FUNC_INFO_TABLE_SIZE);
	if (i13n->fi_table == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"create_fi_table(): Failed to allocate memory.\n");
		return -ENOMEM;
	}
	
	for (i = 0; i < KEDR_FUNC_INFO_TABLE_SIZE; ++i)
		INIT_HLIST_HEAD(&i13n->fi_table[i]);
	
	return 0;
}

/* Destroys the hash table. */
static void
destroy_fi_table(struct kedr_i13n *i13n)
{
	struct hlist_head *head;
	struct hlist_node *tmp;
	struct kedr_fi_table_item *item;
	int i;
	
	if (i13n->fi_table == NULL) 
		return;
	
	for (i = 0; i < KEDR_FUNC_INFO_TABLE_SIZE; ++i) {
		head = &i13n->fi_table[i];
		kedr_hlist_for_each_entry_safe(item, tmp, head, hlist) {
			hlist_del(&item->hlist);
			kfree(item);
		}
	}
	vfree(i13n->fi_table);
	i13n->fi_table = NULL;
}

/* Adds the item to the hash table. 
 * Note that because the table is to be populated in the "on_load" handler
 * for the target module, we may assume that this operation happens in the
 * process context. Therefore we may use GFP_KERNEL here. */
static int
add_item_to_fi_table(struct kedr_i13n *i13n, struct kedr_func_info *fi) 
{
	struct kedr_fi_table_item *item;
	unsigned long bucket;
	
	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (item == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"add_item_to_fi_table(): Failed to allocate memory.\n");
		return -ENOMEM;
	}
	
	INIT_HLIST_NODE(&item->hlist);
	item->fi = fi;
	
	bucket = hash_ptr((void *)fi->addr, KEDR_FUNC_INFO_HASH_BITS);
	hlist_add_head(&item->hlist, &i13n->fi_table[bucket]);
	return 0;
}

struct kedr_func_info *
kedr_i13n_func_info_for_addr(struct kedr_i13n *i13n, unsigned long addr)
{
	struct hlist_head *head;
	struct kedr_fi_table_item *item;
	unsigned long bucket;
	
	bucket = hash_ptr((void *)addr, KEDR_FUNC_INFO_HASH_BITS);
	head = &i13n->fi_table[bucket];
	kedr_hlist_for_each_entry(item, head, hlist) {
		if (item->fi->addr == addr)
			return item->fi;
	}
	return NULL;
}
/* ====================================================================== */

/* CALL/JMP/Jcc near relative (E8, E9 or 0F 8x) */
static int
is_insn_call_or_jxx_rel32(struct insn *insn)
{
	u8 opcode = insn->opcode.bytes[0];
	return (opcode == 0xe8 || opcode == 0xe9 ||
		(opcode == 0x0f && (insn->opcode.bytes[1] & 0xf0) == 0x80));
}
/* ====================================================================== */

/* Relocate the given instruction in the fallback function in place. The 
 * code was "moved" from base address func->info.addr to func->fallback. 
 * [NB] No need to process short jumps outside of the function, they are 
 * already usable. This is because the positions of the functions relative 
 * to each other are the same as for the original functions. */
static int
relocate_insn_in_fallback(struct insn *insn, void *data)
{
	struct kedr_ifunc *func = (struct kedr_ifunc *)data;
	u32 *to_fixup;
	unsigned long addr;
	u32 new_offset;
	BUG_ON(insn->length == 0);
	
	if (is_insn_call_or_jxx_rel32(insn)) {
		/* For calls and jumps, the decoder stores the offset in 
		 * 'immediate' field rather than in 'displacement'.
		 * [NB] When dealing with RIP-relative addressing on x86-64,
		 * it uses 'displacement' field for that purpose. */
		 
		/* Find the new offset corresponding to the same address */
		new_offset = (u32)(func->info.addr + 
			X86_SIGN_EXTEND_V32(insn->immediate.value) -
			(unsigned long)func->fallback);
		
		/* Then calculate the address the instruction refers to.
		 * The original instruction referred to this address too. */
		addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr,
			insn->length, 
			new_offset);
		
		if (kedr_is_address_in_function(addr, func))
		/* no fixup needed, the offset may remain the same */
			return 0; 
		
		/* Call or jump outside of the function. Set the new offset
		 * so that the instruction referred to the same address as 
		 * the original one. */
		to_fixup = (u32 *)((unsigned long)insn->kaddr + 
			insn_offset_immediate(insn));
		*to_fixup = new_offset;
		return 0;
	}

#ifdef CONFIG_X86_64
	if (!insn_rip_relative(insn))
		return 0;

	/* Handle RIP-relative addressing */
	/* Find the new offset first. We assume that the instruction refers
	 * to something outside of the function. The instrumentation system
	 * must have checked this, see ir_node_set_iprel_addr() in ir.c. */
	new_offset = (u32)(func->info.addr + 
		X86_SIGN_EXTEND_V32(insn->displacement.value) -
		(unsigned long)func->fallback);
		
	to_fixup = (u32 *)((unsigned long)insn->kaddr + 
		insn_offset_displacement(insn));
	*to_fixup = new_offset;
#endif
	return 0;
}

/* Performs relocations in the code of the fallback instance of a function. 
 * After that, the instance is ready to be used. */
static int 
relocate_fallback_function(struct kedr_ifunc *func)
{
	return kedr_for_each_insn((unsigned long)func->fallback, 
		(unsigned long)func->fallback + func->size, 
		relocate_insn_in_fallback, 
		(void *)func); 
}

/* Creates an instrumented instance of function specified by 'func' and 
 * prepares the corresponding fallback function for later usage. Note that
 * this function does not prepare jump tables for the fallback instance. */
static int
do_process_function(struct kedr_ifunc *func, struct kedr_i13n *i13n)
{
	int ret = 0;
	struct list_head kedr_ir;
	INIT_LIST_HEAD(&kedr_ir);
	
	BUG_ON(func == NULL || func->info.addr == 0);
	
	/* Small functions should have been removed from the list */
	BUG_ON(func->size < KEDR_SIZE_JMP_REL32);
	
	ret = kedr_ir_create(func, i13n, &kedr_ir);
	if (ret != 0)
		goto out;
	
	/* Call the hook if set. */
	if (core_hooks->on_ir_created != NULL)
		core_hooks->on_ir_created(core_hooks, i13n, func, &kedr_ir);
	
	ret = kedr_ir_instrument(func, &kedr_ir);
	if (ret != 0)
		goto out;
	
	/* Call the hook if set. */
	if (core_hooks->on_ir_transformed != NULL)
		core_hooks->on_ir_transformed(core_hooks, i13n, func, 
			&kedr_ir);
		
	ret = kedr_ir_generate_code(func, &kedr_ir);
	/* [NB] No matter if the code generation has succeeded or failed,
	 * the IR is no longer needed. */
	if (ret != 0)
		goto out;
	
	ret = relocate_fallback_function(func);
out:
	kedr_ir_destroy(&kedr_ir);
	return ret;
}
/* ====================================================================== */

/* Computes the needed size of the detour buffer (the instrumented instances
 * of the functions must have been prepared by this time) and allocates the 
 * buffer. */
static int 
create_detour_buffer(struct kedr_i13n *i13n)
{
	/* Spare bytes to align the start of the buffer, just in case. */
	unsigned long size = KEDR_FUNC_ALIGN; 
	struct kedr_ifunc *f;
	
	list_for_each_entry(f, &i13n->ifuncs, list)
		size += KEDR_ALIGN_VALUE(f->i_size); 
	
	BUG_ON(i13n->detour_buffer != NULL);
	i13n->detour_buffer = kedr_module_alloc(size);
	if (i13n->detour_buffer == NULL)
		return -ENOMEM;
	
	return 0;
}
/* ====================================================================== */

/* The elements of the jump tables are currently the offsets of the jump
 * destinations from the beginning of the instrumented instance. Now that 
 * the base address of that instance function is known (func->i_addr), 
 * these offsets are replaced with the real addresses. */
static void
fixup_instrumented_jump_tables(struct kedr_ifunc *func)
{
	struct kedr_jtable *jtable;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		unsigned int k;
		unsigned long *table = jtable->i_table;
		
		if (table == NULL) {
			BUG_ON(jtable->num != 0);
			continue;
		}
		
		for (k = 0; k < jtable->num; ++k)
			table[k] += (unsigned long)func->i_addr;
	}
}

/* See the description of KEDR_RELOC_IPREL in struct kedr_reloc.
 * The instruction to be relocated can be either call/jmp rel32 or
 * an instruction using RIP-relative addressing.
 * 'dest' is the address the instruction should refer to. */
static void
relocate_iprel_in_icode(struct insn *insn, void *dest)
{
	u32 *to_fixup;
	BUG_ON(insn->length == 0);
	
	if (is_insn_call_or_jxx_rel32(insn)) {
		to_fixup = (u32 *)((unsigned long)insn->kaddr + 
			insn_offset_immediate(insn));
		*to_fixup = (u32)X86_OFFSET_FROM_ADDR(insn->kaddr, 
			insn->length,
			dest);
		return;
	}

#ifdef CONFIG_X86_64
	if (!insn_rip_relative(insn))
		return;

	to_fixup = (u32 *)((unsigned long)insn->kaddr + 
		insn_offset_displacement(insn));
	*to_fixup = (u32)X86_OFFSET_FROM_ADDR(insn->kaddr, insn->length,
		dest);
#endif
}

/* See the description of KEDR_RELOC_ADDR32 in struct kedr_reloc. */
static void
relocate_addr32_in_icode(struct insn *insn)
{
	u32 *to_fixup;
	unsigned long addr;
	
	BUG_ON(insn->length == 0);
	/* imm32 must contain an offset of the memory location which address
	 * is needed. As this type of relocation is expected to be used to 
	 * handle jumps out of the blocks with memory accesses, that offset
	 * must not be 0. This is because such jumps lead to another block 
	 * and there are at least the instructions processing the end of 
	 * the block between the jumps and their destinations. */
	BUG_ON(insn->immediate.value == 0 || insn->immediate.nbytes != 4);
	
	addr = X86_SIGN_EXTEND_V32(insn->immediate.value) + 
		(unsigned long)insn->kaddr + 
		(unsigned long)insn->length;
	
	to_fixup = (u32 *)((unsigned long)insn->kaddr + 
		insn_offset_immediate(insn));
	*to_fixup = (u32)addr;
}

/* Performs fixup of call and jump addresses in the instrumented instance, 
 * as well as RIP-relative addressing, and the contents of the jump tables.
 * Note that the addressing expressions for the jump tables themselves must 
 * be already in place: the instrumentation phase takes care of that. */
static void
deploy_instrumented_function(struct kedr_ifunc *func)
{
	struct kedr_reloc *reloc;
	struct kedr_reloc *tmp;

	fixup_instrumented_jump_tables(func);
	
	/* Decode the instructions that should be relocated and perform 
	 * relocations. Free the relocation structures when done, they are 
	 * no longer needed. */
	list_for_each_entry_safe(reloc, tmp, &func->relocs, list) {
		struct insn insn;
		void *kaddr = (void *)((unsigned long)func->i_addr + 
			reloc->offset);

		BUG_ON(reloc->offset >= func->i_size);
				
		kernel_insn_init(&insn, kaddr);
		insn_get_length(&insn);
		
		if (reloc->rtype == KEDR_RELOC_IPREL)
			relocate_iprel_in_icode(&insn, reloc->dest);
		else if (reloc->rtype == KEDR_RELOC_ADDR32) 
			relocate_addr32_in_icode(&insn);
		else
			BUG(); /* should not get here */
		
		list_del(&reloc->list);
		kfree(reloc);
	}
}

/* Deploys the instrumented code of each function to an appropriate place in
 * the detour buffer. Releases the temporary buffer and sets 'i_addr' to the
 * final address of the instrumented instance. */
static void
deploy_instrumented_code(struct kedr_i13n *i13n)
{
	struct kedr_ifunc *func;
	unsigned long dest_addr;
	
	BUG_ON(i13n->detour_buffer == NULL);
	
	dest_addr = KEDR_ALIGN_VALUE(i13n->detour_buffer);
	list_for_each_entry(func, &i13n->ifuncs, list) {
		BUG_ON(func->tbuf == NULL);
		BUG_ON(func->i_addr != NULL);
		
		memcpy((void *)dest_addr, func->tbuf, func->i_size);
		kfree(func->tbuf);
		func->tbuf = NULL;
		func->i_addr = (void *)dest_addr;
		
		deploy_instrumented_function(func);
		dest_addr += KEDR_ALIGN_VALUE(func->i_size);
	}
}
/* ====================================================================== */

/* Fix up the jump tables for the given function so that the fallback 
 * instance could use them. */
static void
fixup_fallback_jump_tables(struct kedr_ifunc *func, struct kedr_i13n *i13n)
{
	struct kedr_jtable *jtable;
	unsigned long func_start = func->info.addr;
	unsigned long fallback_start = (unsigned long)func->fallback;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		unsigned long *table = jtable->addr;
		unsigned int i;
		/* If the code refers to a "table" without elements (e.g. a 
		 * table filled with the addresses of other functons, etc.),
		 * nothing will be done. 
		 * If the number of the elements is 0 because some other 
		 * jumps use the same jump table, the fixup will be done 
		 * for only one of such jumps, which should be enough. */
		for (i = 0; i < jtable->num; ++i)
			table[i] = table[i] - func_start + fallback_start;
	}
}
/* ====================================================================== */

/* For each original function, place a jump to the instrumented instance at
 * the beginning and fill the rest with '0xcc' (breakpoint) instructions. */
static void
detour_original_functions(struct kedr_i13n *i13n)
{
	u32 *pos;
	struct kedr_ifunc *func;
	
	list_for_each_entry(func, &i13n->ifuncs, list) {
		BUG_ON(func->size < KEDR_SIZE_JMP_REL32);
		
		/* Place the jump to the instrumented instance at the 
		 * beginning of the original instance.
		 * [NB] We allocate memory for the detour buffer in a 
		 * special way, so that it is "not very far" from where the 
		 * code of the target module resides. A near relative jump 
		 * is enough in this case. */
		*(u8 *)func->info.addr = KEDR_OP_JMP_REL32;
		pos = (u32 *)(func->info.addr + 1);
		*pos = X86_OFFSET_FROM_ADDR(func->info.addr, 
			KEDR_SIZE_JMP_REL32, (unsigned long)func->i_addr);

		/* Fill the rest of the original function's code with 
		 * 'int 3' (0xcc) instructions to detect if control still 
		 * transfers there despite all our efforts. 
		 * If we do not handle some situation where the control 
		 * transfers somewhere within an original function rather 
		 * than to its beginning, we better know this early. */
		if (func->size > KEDR_SIZE_JMP_REL32) {
			memset((void *)(func->info.addr + 
					KEDR_SIZE_JMP_REL32), 
				0xcc, 
				func->size - KEDR_SIZE_JMP_REL32);
		}
	}
}
/* ====================================================================== */

static void
on_init_post(struct kedr_local_storage *ls)
{
	kedr_fh_on_init_post(ls->fi->owner);
}

static void
on_exit_pre(struct kedr_local_storage *ls)
{
	kedr_fh_on_exit_pre(ls->fi->owner);
}

static void
set_init_post_callback(struct kedr_i13n *i13n)
{
	struct module *mod = i13n->target;
	struct kedr_func_info *fi;
	unsigned long irq_flags;
	void (*handler)(struct kedr_local_storage *ls) = &on_init_post;
	
	if (mod->init == NULL) 
		return;
	
	/* [NB] kedr_find_func_info() cannot be used here as 'i13n' has not
	 * been saved in the target object yet. */
	fi = kedr_i13n_func_info_for_addr(i13n, (unsigned long)mod->init);
	if (fi == NULL)
		return; /* init() is not instrumentable (e.g., too small) */
	
	/* Use the locks just in case something weird occurs and someone
	 * else tries to set these callbacks even before the target started
	 * executing. Not sure if such situations may occur though. */
	spin_lock_irqsave(&fi->handler_lock, irq_flags);
	rcu_assign_pointer(fi->post_handler, handler);
	spin_unlock_irqrestore(&fi->handler_lock, irq_flags);
}

static void
set_exit_pre_callback(struct kedr_i13n *i13n)
{
	struct module *mod = i13n->target;
	struct kedr_func_info *fi;
	unsigned long irq_flags;
	void (*handler)(struct kedr_local_storage *ls) = &on_exit_pre;
	
	if (mod->exit == NULL) 
		return;

	fi = kedr_i13n_func_info_for_addr(i13n, (unsigned long)mod->exit);
	if (fi == NULL)
		return; /* exit() is not instrumentable (e.g., too small) */
	
	spin_lock_irqsave(&fi->handler_lock, irq_flags);
	rcu_assign_pointer(fi->pre_handler, handler);
	spin_unlock_irqrestore(&fi->handler_lock, irq_flags);
}
/* ====================================================================== */

struct kedr_i13n *
kedr_i13n_process_module(struct module *target)
{
	struct kedr_i13n *i13n;
	struct kedr_ifunc *func;
	int ret = 0;
	
	BUG_ON(target == NULL);
	
	i13n = kzalloc(sizeof(*i13n), GFP_KERNEL);
	if (i13n == NULL) 
		return ERR_PTR(-ENOMEM);
	
	i13n->target = target;
	INIT_LIST_HEAD(&i13n->sections);
	INIT_LIST_HEAD(&i13n->ifuncs);
	
	ret = create_fi_table(i13n);
	if (ret != 0)
		goto out;
	
	ret = alloc_fallback_areas(i13n);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to allocate memory for fallback functions.\n");
		goto out_del_fi;
	}
	
	ret = kedr_get_sections(target, &i13n->sections);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to obtain names and addresses of the target's sections.\n");
		goto out_free_fb;
	}
	
	ret = kedr_get_functions(i13n);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to prepare the list of functions to be processed.\n");
		goto out_free_sections;
	}
	/* If there are no instrumentable functions, nothing to do. */
	if (list_empty(&i13n->ifuncs)) 
		return i13n;
	
	list_for_each_entry(func, &i13n->ifuncs, list) {
		ret = do_process_function(func, i13n);
		if (ret != 0) {
			pr_warning(KEDR_MSG_PREFIX 
				"Failed to instrument function %s().\n",
				func->name);
			goto out_free_functions;
		}
		
		ret = add_item_to_fi_table(i13n, &func->info);
		if (ret != 0) {
			pr_warning(KEDR_MSG_PREFIX 
			"Failed to add data for %s() to the hash table.\n",
				func->name);
			goto out_free_functions;
		}
	}
	
	/* Calculate the total size of the original functions and of their
	 * instrumented instances (for statistics). Both values are 
	 * initially 0. */
	list_for_each_entry(func, &i13n->ifuncs, list) {
		i13n->total_size += func->size;
		i13n->total_i_size += func->i_size;
	}
	pr_info(KEDR_MSG_PREFIX "Total size of the functions before "
		"instrumentation (bytes): %lu, after: %lu\n",
		i13n->total_size, i13n->total_i_size);
	
	ret = create_detour_buffer(i13n);
	if (ret != 0)
		goto out_free_functions;

	deploy_instrumented_code(i13n);

	/* At this point, nothing more should fail, so we can finally 
	 * fixup the jump tables to be applicable for the fallback instances
	 * rather than for the original one. */
	list_for_each_entry(func, &i13n->ifuncs, list)
		fixup_fallback_jump_tables(func, i13n);
	
	detour_original_functions(i13n);
	
	set_init_post_callback(i13n);
	set_exit_pre_callback(i13n);
	return i13n;

out_free_functions:
	kedr_release_functions(i13n);
out_free_sections:
	kedr_release_sections(&i13n->sections);
out_free_fb:
	free_fallback_areas(i13n);
out_del_fi:
	destroy_fi_table(i13n);
out:
	kedr_module_free(i13n->detour_buffer); /* just in case */
	kfree(i13n);
	return ERR_PTR(ret);
}

void
kedr_i13n_cleanup(struct kedr_i13n *i13n)
{
	BUG_ON(i13n == NULL);
	
	destroy_fi_table(i13n);
	
	kedr_release_functions(i13n);
	kedr_module_free(i13n->detour_buffer);
	i13n->detour_buffer = NULL;
	
	kedr_release_sections(&i13n->sections);
	free_fallback_areas(i13n);
	kfree(i13n);
}
