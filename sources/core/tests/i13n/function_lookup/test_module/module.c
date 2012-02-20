/* This module is used when testing function lookup facilities of the core. 
 * The module outputs information about the functions found in the target 
 * module to a file in debugfs. For each function, the following data are 
 * output: name, size, name of the ELF section the function belongs to, 
 * offset of the function in that section.
 *
 * [NB] This module itself does not perform any tests, it just provides data
 * for analysis in the user space. 
 *
 * For each function to be instrumented in the target module, a line is 
 * printed to a file in debugfs. 
 * The format is as follows, the fields are separated by spaces: 
 *
 * <name> <size> <section_name> <offset_in_section>
 *
 * "<name>" and "<section_name>" are strings, "<size>" is a decimal, 
 * "<offset_in_section>" is a hexadecimal number. */

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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/sort.h>

#include "config.h"
#include "core_impl.h"

#include "debug_util.h"
#include "hooks.h"
#include "i13n.h"
#include "sections.h"
#include "ifunc.h"

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* A directory for the core in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "test_function_lookup";
/* ====================================================================== */

/* An array of pointers to sections sorted by the start addresses of the 
 * sections in ascending order. It is needed to determine which section a 
 * function belongs to. */
static struct kedr_section **sptr = NULL;
static size_t num_sections = 0;
/* ====================================================================== */

static void 
test_on_func_lookup(struct kedr_core_hooks *hooks, struct kedr_i13n *i13n);

struct kedr_core_hooks test_hooks = {
	.owner = THIS_MODULE,
	.on_func_lookup_completed = test_on_func_lookup,
};
/* ====================================================================== */

/* Find the sections the given function belongs to. 'sptr' should be already
 * sorted before this function is called. */
static struct kedr_section *
get_section(struct kedr_ifunc *func)
{
	size_t i;
	unsigned long addr = (unsigned long)func->addr;
	
	BUG_ON(num_sections == 0);
	
	if (addr < sptr[0]->addr)
		return NULL; /* unknown section */
		
	for (i = 1; i < num_sections; ++i) {
		if (addr >= sptr[i - 1]->addr && addr < sptr[i]->addr)
			return sptr[i - 1];
	}
	return sptr[num_sections - 1];
}

static void
print_func_info(struct kedr_ifunc *func)
{
	struct kedr_section *sec;
	sec = get_section(func);
	
	debug_util_print_string(func->name);
	debug_util_print_u64((u64)func->size, " %llu ");
	
	if (sec != NULL) {
		debug_util_print_string(sec->name);
		debug_util_print_u64((u64)((unsigned long)func->addr - 
			sec->addr), " %llx\n");
	}
	else {
		/* Unknown section. Output 0 as the offset, the value does 
		 * not really matter. */
		debug_util_print_string("unknown 0\n");
	}
}

/* A comparator for sorting. */
static int 
compare_items(const void *lhs, const void *rhs)
{
	const struct kedr_section *left = 
		*(const struct kedr_section **)lhs;
	const struct kedr_section *right = 
		*(const struct kedr_section **)rhs;
	
	if (left->addr < right->addr) {
		return -1;
	}
	else if (left->addr > right->addr) {
		return 1;
	}
	else {
		/* just in case an element is compared to itself */
		return 0; 
	}
}

static void 
swap_items(void *lhs, void *rhs, int size)
{
	struct kedr_section **pleft = (struct kedr_section **)lhs;
	struct kedr_section **pright = (struct kedr_section **)rhs;
	struct kedr_section *p;
	
	BUG_ON(size != (int)sizeof(struct kedr_section *));
	p = *pleft;
	*pleft = *pright;
	*pright = p;
}

static void 
test_on_func_lookup(struct kedr_core_hooks *hooks, struct kedr_i13n *i13n)
{
	struct list_head *tmp = NULL;
	struct kedr_section *sec = NULL;
	struct kedr_ifunc *func = NULL;
	int i = 0;
	
	BUG_ON(hooks != &test_hooks);
	BUG_ON(i13n == NULL);
	
	debug_util_clear();
	
	num_sections = 0;
	list_for_each(tmp, &i13n->sections)
		++num_sections;
	if (num_sections == 0) {
		pr_warning(KEDR_MSG_PREFIX 
			"No loaded sections found in the target module.\n");
		return;
	}
	
	sptr = kzalloc(num_sections * sizeof(sptr[0]), GFP_KERNEL);
	if (sptr == NULL) {
		pr_warning(KEDR_MSG_PREFIX "Out of memory.\n");
		return;
	}
	list_for_each_entry(sec, &i13n->sections, list) {
		sptr[i] = sec;
		++i;
	}
	sort(sptr, num_sections, sizeof(struct kedr_section *), 
		compare_items, swap_items);
		
	list_for_each_entry(func, &i13n->ifuncs, list)
		print_func_info(func);
	
	kfree(sptr);
}
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_set_core_hooks(NULL);
	debug_util_fini();
	debugfs_remove(debugfs_dir_dentry);
	return;
}

static int __init
test_init_module(void)
{
	int ret = 0;
	
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (debugfs_dir_dentry == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"failed to create a directory in debugfs\n");
		ret = -EINVAL;
		goto out;
	}
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_warning(KEDR_MSG_PREFIX "debugfs is not supported\n");
		ret = -ENODEV;
		goto out;
	}

	ret = debug_util_init(debugfs_dir_dentry);
	if (ret != 0)
		goto out_rmdir;
	
	kedr_set_core_hooks(&test_hooks);
	return 0;

out_rmdir:
	debugfs_remove(debugfs_dir_dentry);
out:
	return ret;	
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
