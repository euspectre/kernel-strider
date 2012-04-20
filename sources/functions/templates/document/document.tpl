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
#include <linux/errno.h>
#include <linux/sort.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#include "config.h"

<$if concat(header)$><$header: join(\n)$><$endif$>
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_func_drd]"
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

/* The map <target function; handlers>. This array is only changed in the
 * init function where it is sorted. After that it remains the same and
 * can be read from any number of threads simultaneously without locking. */
static struct kedr_func_drd_handlers handlers[] = {
<$handlerItem: join(\n)$>
};
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
		if (func < handlers[mid].func)
			end = mid;
		else if (func > handlers[mid].func)
			beg = mid + 1;
		else 
			return &handlers[mid];
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

/* Needed only to sort the array during module init. */
static int __init
compare_func(const void *lhs, const void *rhs)
{
	const struct kedr_func_drd_handlers *left = 
		(const struct kedr_func_drd_handlers *)lhs;
	const struct kedr_func_drd_handlers *right = 
		(const struct kedr_func_drd_handlers *)rhs;
	
	if (left->func < right->func)
		return -1;
	else if (left->func > right->func)
		return 1;
	else 
		return 0;
}

/* Needed only to sort the array during module init. */
static void __init
swap_func(void *lhs, void *rhs, int size)
{
	struct kedr_func_drd_handlers *left = 
		(struct kedr_func_drd_handlers *)lhs;
	struct kedr_func_drd_handlers *right = 
		(struct kedr_func_drd_handlers *)rhs;
	struct kedr_func_drd_handlers t;
	
	BUG_ON(size != (int)sizeof(struct kedr_func_drd_handlers));
	
	t.func = left->func;
	t.pre_handler = left->pre_handler;
	t.post_handler = left->post_handler;
	
	left->func = right->func;
	left->pre_handler = right->pre_handler;
	left->post_handler = right->post_handler;
	
	right->func = t.func;
	right->pre_handler = t.pre_handler;
	right->post_handler = t.post_handler;
}

static int __init
func_drd_init_module(void)
{
	int ret = 0;
	
	/* Sort the array in ascending order by the function addresses. */
	sort(&handlers[0], ARRAY_SIZE(handlers), 
		sizeof(struct kedr_func_drd_handlers), 
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
