/* This module is responsible for calling appropriate pre and post handlers
 * for some of the functions called by the target module, namely: 
 * lock/unlock operations, alloc/free, and may be some more. 
 *
 * The focus here is on the functions that could be interesting when 
 * detecting data races, hence "func_drd" in the names.
 * 
 * See also on_*_pre() / on_*_post() in core_api.h.
 *
 * No replacement is provided here for the target functions, so the latter
 * should execute as they are. */
	
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/sort.h>
#include <linux/kallsyms.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#include "config.h"

<$if concat(header)$><$header: join(\n)$><$endif$>
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_func_drd] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

<$handlerDecl: join(\n\n)$>
/* ====================================================================== */

struct kedr_func_drd_handlers
{
	/* The address of the target function. */
	unsigned long func; 
	
	/* The handlers. */
	void (*pre_handler)(struct kedr_local_storage *);
	void (*post_handler)(struct kedr_local_storage *);
};
/* ====================================================================== */

<$block: join(\n)$>
/* ====================================================================== */

/* The map <target function; handlers>. This array is only changed in the
 * init function where it is sorted. After that it remains the same and
 * can be read from any number of threads simultaneously without locking. */
static struct kedr_func_drd_handlers *handlers[] = {
	<$handlerItem: join(,\n\t)$>
};
/* ====================================================================== */

/* For some of the functions we need to process, we cannot take the address
 * explicitly. For example, a compiler may not allow to use '&memset' if 
 * "memset" function, "memset" macro and/or "memset" builtin co-exist for
 * the sake of optimization, etc. However, to properly process the 
 * situations where the function is actually called, we need the address
 * of that function. This module will lookup the addresses of such 
 * functions via "kallsyms" during the initialization (see below). 
 * Note that the instances of "struct kedr_func_drd_to_lookup" must not
 * be used after this module has completed its initialization. */
struct kedr_func_drd_to_lookup
{
	const char *name;
	struct kedr_func_drd_handlers *h;
};

/* [NB] The last ("NULL") element is here for the array not to be empty */
static struct kedr_func_drd_to_lookup __initdata to_lookup[] = {
<$lookupItem: join(\n)$>	{NULL, NULL} 
};
/* ====================================================================== */

/* A comparator, a swap function and a search function for 'to_lookup[]' 
 * array. Needed only during module init. */
static int __init
to_lookup_compare(const void *lhs, const void *rhs)
{
	const struct kedr_func_drd_to_lookup *left = 
		(const struct kedr_func_drd_to_lookup *)lhs;
	const struct kedr_func_drd_to_lookup *right = 
		(const struct kedr_func_drd_to_lookup *)rhs;
	int result;
	
	BUILD_BUG_ON(ARRAY_SIZE(to_lookup) < 1);
	
	result = strcmp(left->name, right->name);
	if (result > 0)
		return 1;
	else if (result < 0)
		return -1;
	return 0;
}

static void __init
to_lookup_swap(void *lhs, void *rhs, int size)
{
	struct kedr_func_drd_to_lookup *left = 
		(struct kedr_func_drd_to_lookup *)lhs;
	struct kedr_func_drd_to_lookup *right = 
		(struct kedr_func_drd_to_lookup *)rhs;
	struct kedr_func_drd_to_lookup t;
	
	BUG_ON(size != (int)sizeof(struct kedr_func_drd_to_lookup));
	
	t.name = left->name;
	t.h = left->h;
	
	left->name = right->name;
	left->h = right->h;
	
	right->name = t.name;
	right->h = t.h;
}

/* Checks if 'to_lookup[]' contains a record for a function with the given
 * name. Returns the pointer to the record if found, NULL otherwise. 
 * 'to_lookup[]' must be sorted by function name in ascending order by this
 *  time */
static struct kedr_func_drd_to_lookup * __init
to_lookup_search(const char *name)
{
	size_t beg = 0;
	size_t end = ARRAY_SIZE(to_lookup) - 1;
		
	BUILD_BUG_ON(ARRAY_SIZE(to_lookup) < 1);
	
	while (beg < end) {
		int result;
		size_t mid = beg + (end - beg) / 2;

		result = strcmp(name, to_lookup[mid].name);
		if (result < 0)
			end = mid;
		else if (result > 0)
			beg = mid + 1;
		else 
			return &to_lookup[mid];
	}
	return NULL;
}
/* ====================================================================== */

/* Searches for the handlers for the function 'func' in the collection of
 * the known handlers. Returns a pointer to the corresponding item if found,
 * NULL otherwise. 
 *
 * [NB] The linux kernel provides bsearch() function but unfortunately, only
 * starting from version 3.0. So we implement it here. */
static struct kedr_func_drd_handlers *
lookup_handlers(unsigned long func)
{
	size_t beg = 0;
	size_t end = ARRAY_SIZE(handlers);
	
	/* The handlers[] array must have been sorted by this time, so we
	 * can use binary search. 
	 * At each iteration, [beg, end) range of indexes is considered. */
	while (beg < end) {
		size_t mid = beg + (end - beg) / 2;
		if (func < handlers[mid]->func)
			end = mid;
		else if (func > handlers[mid]->func)
			beg = mid + 1;
		else 
			return handlers[mid];
	}
	
	return NULL;
}
/* ====================================================================== */

static int 
fill_call_info(struct kedr_function_handlers *fh, 
	struct kedr_call_info *call_info)
{
	struct kedr_func_drd_handlers *h;
	h = lookup_handlers(call_info->target);
	if (h == NULL)
		return 0;
	
	/* We do not need a replacement. */
	call_info->repl = call_info->target;
	
	/* Found appropriate handlers */
	call_info->pre_handler = h->pre_handler;
	call_info->post_handler = h->post_handler;
	return 1;
}

static struct kedr_function_handlers fh = {
	.owner = THIS_MODULE,
	.fill_call_info = fill_call_info,
};
/* ====================================================================== */

/* Needed only to sort 'handlers[]' array during module init. */
static int __init
compare_func(const void *lhs, const void *rhs)
{
	const struct kedr_func_drd_handlers *left = 
		*(const struct kedr_func_drd_handlers **)lhs;
	const struct kedr_func_drd_handlers *right = 
		*(const struct kedr_func_drd_handlers **)rhs;
	
	BUG_ON(left->func == 0);
	BUG_ON(right->func == 0);
	
	if (left->func < right->func)
		return -1;
	else if (left->func > right->func)
		return 1;
	else 
		return 0;
}

/* Needed only to sort 'handlers[]' array during module init. */
static void __init
swap_func(void *lhs, void *rhs, int size)
{
	struct kedr_func_drd_handlers **pleft = 
		(struct kedr_func_drd_handlers **)lhs;
	struct kedr_func_drd_handlers **pright = 
		(struct kedr_func_drd_handlers **)rhs;
	struct kedr_func_drd_handlers *t;
	
	BUG_ON(size != (int)sizeof(struct kedr_func_drd_handlers *));
	
	t = *pleft;
	*pleft = *pright;
	*pright = t;
}
/* ====================================================================== */

/* This function will be called for each symbol known to the system to 
 * find the addresses of the functions listed in 'to_lookup[]'.
 *
 * If this function returns 0, kallsyms_on_each_symbol() will continue
 * walking the symbols. If non-zero - it will stop. */
static int __init
symbol_walk_callback(void *data, const char *name, struct module *mod, 
	unsigned long addr)
{
	struct kedr_func_drd_to_lookup *item;
	
	/* Skip the symbol if it belongs to a module rather than to 
	 * the kernel proper. */
	if (mod != NULL) 
		return 0;
	
	item = to_lookup_search(name);
	if (item == NULL)
		return 0;
	
	item->h->func = addr;
	return 0;
}
/* ====================================================================== */

static int __init
func_drd_init_module(void)
{
	int ret = 0;
	size_t size = ARRAY_SIZE(to_lookup) - 1;
	size_t i;
		
	/* Sort 'to_lookup[]' array (except the "NULL" element) in 
	 * ascending order by the function names. */
	sort(&to_lookup[0], size, sizeof(struct kedr_func_drd_to_lookup),
		to_lookup_compare, to_lookup_swap);
	
	ret = kallsyms_on_each_symbol(symbol_walk_callback, NULL);
	if (ret != 0)
		return ret;
	
	/* Check that all the required addresses have been found. */
	for (i = 0; i < size; ++i) {
		if (to_lookup[i].h->func == 0) {
			pr_warning(KEDR_MSG_PREFIX 
			"Unable to find the address of %s function",
				to_lookup[i].name);
			return -EFAULT;
		}
	}
	
	/* Sort 'handlers[]' array in ascending order by the function 
	 * addresses. */
	sort(&handlers[0], ARRAY_SIZE(handlers), 
		sizeof(struct kedr_func_drd_handlers *), 
		compare_func, swap_func);
	
	ret = kedr_set_function_handlers(&fh);	
	return ret;
}

static void __exit
func_drd_exit_module(void)
{
	kedr_set_function_handlers(NULL);
	
	/* [NB] If additional cleanup is needed, do it after the handlers
	 * have been reset to the defaults. */
	return;
}

module_init(func_drd_init_module);
module_exit(func_drd_exit_module);
/* ====================================================================== */
