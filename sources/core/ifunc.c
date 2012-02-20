/* ifunc.c - API to manage the collection of the functions found in the 
 * target module. */

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

#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/sort.h>
#include <linux/slab.h>

#include <kedr/asm/insn.h>
#include <kedr/kedr_mem/block_info.h>

#include "core_impl.h"
#include "config.h"

#include "ifunc.h"
#include "i13n.h"
#include "module_ms_alloc.h"
#include "util.h"
#include "sections.h"
#include "hooks.h"
/* ====================================================================== */

/* Common aliases: init and cleanup functions of a module. */
static const char *name_module_init = "init_module";
static const char *name_module_exit = "cleanup_module";
/* ====================================================================== */

/* For a given function, free the structures related to the jump tables for
 * the corresponding instrumented instance.
 * Deallocate and remove all members from 'func->jump_tables' as well. */
static void 
cleanup_jump_tables(struct kedr_ifunc *func)
{
	struct kedr_jtable *jtable;
	struct kedr_jtable *jtmp;
	
	list_for_each_entry_safe(jtable, jtmp, &func->jump_tables, list) {
		list_del(&jtable->list);
		kfree(jtable);
	}
	
	kedr_module_free(func->jt_buf);
	func->jt_buf = NULL;
}

static void
cleanup_relocs(struct list_head *relocs)
{
	struct kedr_reloc *reloc;
	struct kedr_reloc *tmp;
	
	BUG_ON(relocs == NULL);
	
	list_for_each_entry_safe(reloc, tmp, relocs, list) {
		list_del(&reloc->list);
		kfree(reloc);
	}
}

static void
cleanup_block_infos(struct list_head *block_infos)
{
	struct kedr_block_info *bi;
	struct kedr_block_info *tmp;
	
	BUG_ON(block_infos == NULL);
	
	list_for_each_entry_safe(bi, tmp, block_infos, list) {
		list_del(&bi->list);
		kfree(bi);
	}
}

static void
ifunc_destroy(struct kedr_ifunc *func)
{
	cleanup_jump_tables(func);
	cleanup_relocs(&func->relocs);
	cleanup_block_infos(&func->block_infos);
	
	/* If the instrumentation completed successfully, func->tbuf must be 
	 * NULL. If an error occurred during the instrumentation, the
	 * temporary buffer for the instrumented instance may have remained
	 * unfreed. Free it now. */
	kfree(func->tbuf);
	kfree(func);
}
/* ====================================================================== */

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module in the "init" area, 0 otherwise. */
static int
is_init_text_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);
	if ((mod->module_init != NULL) &&
	    (addr >= (unsigned long)(mod->module_init)) &&
	    (addr < (unsigned long)(mod->module_init) + mod->init_text_size))
		return 1;
	
	return 0;
}

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module in the "core" area, 0 otherwise. */
static int
is_core_text_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);

	if ((mod->module_core != NULL) &&
	    (addr >= (unsigned long)(mod->module_core)) &&
	    (addr < (unsigned long)(mod->module_core) + mod->core_text_size))
		return 1;
	
	return 0;
}

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module (*.text* sections), 0 otherwise. */
static int
is_text_address(unsigned long addr, struct module *mod)
{
	return (is_core_text_address(addr, mod) || 
		is_init_text_address(addr, mod));
}

/* Given the address of a memory location ('orig_addr') in an original 
 * memory area (the area starts at 'orig_area') and the start address of the
 * fallback memory area, determine the corresponding address in the latter.
 * The offset of the location is the same in both areas. */
static void *
fallback_address(void *orig_addr, void *orig_area, void *fallback_area)
{
	return (void *)((unsigned long)fallback_area +
		((unsigned long)orig_addr - (unsigned long)orig_area));
}

/* Prepares the structures needed to instrument the given function.
 * Called for each function found in the target module.
 * 
 * Returns 0 if the processing succeeds, error otherwise.
 * This error will be propagated to the return value of 
 * kallsyms_on_each_symbol() */
static int
do_prepare_function(struct kedr_i13n *i13n, const char *name, 
	unsigned long addr)
{
	struct module *mod = i13n->target;
	struct kedr_ifunc *tf;
	
	tf = kzalloc(sizeof(*tf), GFP_KERNEL);
	if (tf == NULL)
		return -ENOMEM;
	
	tf->addr = (void *)addr; /* [NB] tf->size is 0 now */
	tf->name = name;

	INIT_LIST_HEAD(&tf->jump_tables);
	INIT_LIST_HEAD(&tf->relocs);
	INIT_LIST_HEAD(&tf->block_infos);

	/* Find the corresponding fallback function, it's at the same offset 
	 * from the beginning of fallback_init_area or fallback_core_area as
	 * the original function is from the beginning of init or core area
	 * in the module, respectively. */
	if (is_core_text_address(addr, mod)) {
		tf->fallback = fallback_address((void *)addr, 
			mod->module_core, i13n->fallback_core_area);
	} 
	else if (is_init_text_address(addr, mod)) {
		tf->fallback = fallback_address((void *)addr,
			mod->module_init, i13n->fallback_init_area);
	}
	else {	
		/* Must not get here */
		BUG();
	}
	
	list_add(&tf->list, &i13n->ifuncs);
	++i13n->num_ifuncs;
	return 0;
}

/* This function will be called for each symbol known to the system.
 * We need to find only functions and only from the target module.
 *
 * If this function returns 0, kallsyms_on_each_symbol() will continue
 * walking the symbols. If nonzero - it will stop. */
static int
symbol_walk_callback(void *data, const char *name, struct module *mod, 
	unsigned long addr)
{
	struct kedr_i13n *i13n = (struct kedr_i13n *)data;
	struct module *target;
	target = i13n->target;
	
	/* For now it seems to be enough to compare only addresses of 
	 * struct module instances for the target module and the module
	 * the current symbol belongs to. */
	if (mod == target && 
	    name[0] != '\0' && /* skip symbols with empty name */
	    is_text_address(addr, mod) && 
	    strcmp(name, name_module_init) != 0 &&  /* skip these aliases */
	    strcmp(name, name_module_exit) != 0) {
	 	int ret = do_prepare_function(i13n, name, addr);
	 	if (ret)
			return ret;
	}
	return 0;
}

/* Deletes all the special items (see create_special_items()) from the list 
 * and frees them. */
static void
destroy_special_items(struct list_head *items_list)
{
	struct kedr_ifunc *pos;
	struct kedr_ifunc *tmp;
	
	BUG_ON(items_list == NULL);
	
	list_for_each_entry_safe(pos, tmp, items_list, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}

/* Allocates a special item and sets 'addr' field in it to the given value. 
 * Returns the pointer to the item if successful, NULL if there is not 
 * enough memory. 
 * 
 * [NB] For special items, it is enough to set the address only. */
static struct kedr_ifunc *
construct_special_item(unsigned long addr)
{
	struct kedr_ifunc *item;
	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (item == NULL)
		return NULL;
	
	item->addr = (void *)addr;
	return item;
}

/* Creates the list of the special items, i.e. the instances of 
 * struct kedr_ifunc representing sections and the ends of the areas of the
 * given module. 
 * The items will be added to the specified list (must be empty on entry).
 * If successful, the function returns the number of items created (>= 0).
 * A negative error code is returned in case of failure. */
static int
create_special_items(struct kedr_i13n *i13n, struct list_head *items_list)
{
	int num = 0;
	int ret = 0;
	struct module *target = i13n->target;
	struct kedr_section *sec;
	struct kedr_ifunc *item;
	
	BUG_ON(items_list == NULL);
	BUG_ON(!list_empty(items_list));
	BUG_ON(list_empty(&i13n->sections)); 
	
	list_for_each_entry(sec, &i13n->sections, list) {
		item = construct_special_item(sec->addr);
		if (item == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		list_add_tail(&item->list, items_list);
		++num;
	}
	
	/* Here we rely on the fact that the code is placed at the beginning
	 * of "init" and "core" areas of the module by the module loader. 
	 * To estimate the sizes of the functions, we therefore need the 
	 * "end addresses" (more exactly, start address + size) of these 
	 * areas among other things. */
	if (target->module_init != NULL) {
		item = construct_special_item(
			(unsigned long)target->module_init + 
			target->init_text_size);
		if (item == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		list_add_tail(&item->list, items_list);
		++num;
	}

	if (target->module_core != NULL) {
		item = construct_special_item(
			(unsigned long)target->module_core + 
			target->core_text_size);
		if (item == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		list_add_tail(&item->list, items_list);
		++num;
	}
	return num;

out:
	destroy_special_items(items_list);
	return ret;
}

/* A helper structure that is used to implement stable sorting of function
 * boundaries (to determine function size later). 
 * '*obj' may represent a function or a special item (like the end of "init"
 * or "core" area or a start of an ELF section). 
 * 'index' is the original index of an item, i.e. the index in the array 
 * before sorting. */
struct func_boundary_item
{
	struct kedr_ifunc *obj;
	int index;
};

/* Compare pairs (addr, index) in a lexicographical order. This is 
 * to make sorting stable, that is, to preserve the order of the elements
 * with equal values of 'addr'. 
 * 'index' is the index of the element in the array before sorting. */
static int 
compare_items(const void *lhs, const void *rhs)
{
	const struct func_boundary_item *left = 
		(const struct func_boundary_item *)lhs;
	const struct func_boundary_item *right = 
		(const struct func_boundary_item *)rhs;
	
	if (left->obj->addr == right->obj->addr) {
		if (left->index == right->index)
		/* may happen only if an element is compared to itself */
			return 0; 
		else if (left->index < right->index)
			return -1;
		else 
			return 1;
	}
	else if (left->obj->addr < right->obj->addr)
		return -1;
	else 
		return 1;
}

static void 
swap_items(void *lhs, void *rhs, int size)
{
	struct func_boundary_item *left = 
		(struct func_boundary_item *)lhs;
	struct func_boundary_item *right = 
		(struct func_boundary_item *)rhs;
	struct func_boundary_item p;
	
	BUG_ON(size != (int)sizeof(struct func_boundary_item));
	
	p.obj = left->obj;
	p.index = left->index;
	
	left->obj = right->obj;
	left->index = right->index;
	
	right->obj = p.obj;
	right->index = p.index;
}

/* Remove the elements for the functions shorter than the length of 
 * 'jmp near rel32' from the list of functions and destroy them.
 * 
 * The elements with zero size may appear if there are aliases for one or 
 * more functions, that is, if there are symbols with the same start 
 * address. When doing the instrumentation, we need to process only one 
 * function of each such group, no matter which one exactly. Note that the
 * common aliases ("init_module" and "cleanup_module") should have been 
 * already filtered out by now. So we can safely remove all the elements
 * with zero size.
 * 
 * The functions with the size less than the size of 'jmp near rel32' can 
 * not be detoured (not enough space to place a jump in). We remove them 
 * from the list too. 
 * 
 * [NB] It is recommended to decode the instructions in the functions and
 * adjust 'size' fields of the elements before calling this function. */ 
static void
remove_aliases_and_small_funcs(struct kedr_i13n *i13n)
{
	struct kedr_ifunc *pos;
	struct kedr_ifunc *tmp;
	
	list_for_each_entry_safe(pos, tmp, &i13n->ifuncs, list) {
		if (pos->size < KEDR_SIZE_JMP_REL32) {
			list_del(&pos->list);
			kfree(pos);
			--i13n->num_ifuncs;
		}
	}
}

/* Skip trailing zeros. If these are a part of an instruction, it will be 
 * handled later in do_adjust_size(). If it just a padding sequence, it 
 * should not count as a part of the function.
 * It is unlikely that a function ends with an instruction 'add %al, %(eax)'
 * (code: 0x0000) that is not a padding. */
static void
skip_trailing_zeros(struct kedr_ifunc *func)
{
	if (func->size == 0)
		return; /* nothing to do */

	while (func->size != 0 && 
		*(u8 *)((unsigned long)func->addr + func->size - 1) == '\0')
		--func->size;
}

/* If we have skipped too many zeros at the end of the function, that is, 
 * if we have cut off a part of the last instruction, fix it now. */
static int
do_adjust_size(struct kedr_ifunc *func, struct insn *insn, void *unused)
{
	unsigned long start_addr;
	unsigned long offset_after_insn;
	
	start_addr = (unsigned long)func->addr;
	offset_after_insn = (unsigned long)insn->kaddr + 
		(unsigned long)insn->length - start_addr;
		
	/* If we've got too far, probably there is a bug in our system. It 
	 * is impossible for an instruction to be located at 64M distance
	 * or further from the beginning of the corresponding function
	 * because of the limit on the code size. */
	BUG_ON(offset_after_insn >= 0x04000000UL);
	
	if (offset_after_insn > func->size)
		func->size = offset_after_insn;

	return 0; 
}

/* Decode the instructions in the given function and adjust its estimated 
 * size taking possible padding into account. */
static int 
adjust_size(struct kedr_ifunc *func)
{
	BUG_ON(func == NULL);
	
	skip_trailing_zeros(func);
	if (func->size == 0)
		return 0;
	return kedr_for_each_insn_in_function(func, do_adjust_size, NULL);
}

/* Find the functions in the original code and find the addresses of the 
 * corresponding fallback functions. Create and partially initialize
 * 'struct kedr_ifunc' instances, add them to 'i13n->ifuncs' list. */
static int
find_functions(struct kedr_i13n *i13n)
{
	struct module *target = i13n->target;
	struct func_boundary_item *func_boundaries = NULL;
	LIST_HEAD(special_items);
	struct kedr_ifunc *pos;
	int ret; 
	int i;
	int num_special;
	
	ret = kallsyms_on_each_symbol(symbol_walk_callback, (void *)i13n);
	if (ret)
		return ret;
	
	if (i13n->num_ifuncs == 0) {
		pr_info(KEDR_MSG_PREFIX
			"No functions found in \"%s\", nothing to do\n",
			module_name(target));
		return 0;
	} 
	
	ret = create_special_items(i13n, &special_items);
	if (ret < 0)
		return ret;
	
	num_special = ret;
	/* num_special should not be 0: at least "core" area should be 
	 * present. If it is 0, it is weird. */
	WARN_ON(num_special == 0); 
	
	/* This array is only necessary to estimate the size of each 
	 * function. */
	func_boundaries = kzalloc(sizeof(struct func_boundary_item) * 
			(i13n->num_ifuncs + num_special),
		GFP_KERNEL);
		
	if (func_boundaries == NULL) {
		destroy_special_items(&special_items);
		ret = -ENOMEM;
		goto out;
	}
	
	/* We add special items before the regular functions. Because of the 
	 * fact that the way of sorting we use is stable, the special items
	 * having the same address as a function will still appear before 
	 * the function in the sorted array. 
	 * 
	 * Note that sort() is not required to be stable by itself. */
	i = 0;
	list_for_each_entry(pos, &special_items, list) {
		func_boundaries[i].obj = pos;
		func_boundaries[i].index = i;
		++i;
	}
	BUG_ON(i != num_special);
	
	list_for_each_entry(pos, &i13n->ifuncs, list) {
		func_boundaries[i].obj = pos;
		func_boundaries[i].index = i;
		++i;
	}
	BUG_ON(i != (int)i13n->num_ifuncs + num_special);
	--i;
	
	sort(func_boundaries, (size_t)(i13n->num_ifuncs + num_special), 
		sizeof(struct func_boundary_item), 
		compare_items, swap_items);

	while (i-- > 0) {
		func_boundaries[i].obj->size = 
			((unsigned long)(func_boundaries[i + 1].obj->addr) - 
			(unsigned long)(func_boundaries[i].obj->addr));
	}
	kfree(func_boundaries);
	destroy_special_items(&special_items);
	
	list_for_each_entry(pos, &i13n->ifuncs, list) {
		ret = adjust_size(pos);
		if (ret != 0) {
			pr_warning(KEDR_MSG_PREFIX 
			"Failed to decode function \"%s\"\n", pos->name);
			goto out;
		}
	}
	remove_aliases_and_small_funcs(i13n);
	
	if (list_empty(&i13n->ifuncs)) {
		pr_info(KEDR_MSG_PREFIX 
		"No functions found in \"%s\" that can be instrumented\n",
			module_name(target));
	}
	else {
		pr_info(KEDR_MSG_PREFIX 
		"Number of functions to be instrumented in \"%s\": %u\n",
			module_name(target), i13n->num_ifuncs);
	}
	return 0;
out:
	/* If an error occured above, some elements might still have been
	 * added to the list of functions. Clean it up. */
	kedr_release_functions(i13n);
	return ret;
}
/* ====================================================================== */

int
kedr_get_functions(struct kedr_i13n *i13n)
{
	int ret = 0;
	
	BUG_ON(i13n == NULL);
	
	/* The section lookup subsystem must be called before 
	 * kedr_get_functions(). That subsystem either returns an error or a 
	 * non-empty list of sections. We must not get here if the list of 
	 * sections is empty. */
	BUG_ON(list_empty(&i13n->sections));
	
	ret = find_functions(i13n);
	if (ret != 0)
		return ret;
	
	/* Call the hook if set. */
	if (core_hooks->on_func_lookup_completed != NULL)
		core_hooks->on_func_lookup_completed(core_hooks, i13n);
	return 0;
}

void
kedr_release_functions(struct kedr_i13n *i13n)
{
	struct kedr_ifunc *func;
	struct kedr_ifunc *tmp;
	
	list_for_each_entry_safe(func, tmp, &i13n->ifuncs, list) {
		list_del(&func->list);
		ifunc_destroy(func);
	}
	i13n->num_ifuncs = 0; /* just in case */
}
/* ====================================================================== */