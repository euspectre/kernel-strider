/* fh_impl.c - basic operations needed to support function handling
 * plugins. */

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
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sort.h>

#include <kedr/kedr_mem/functions.h>

#include "config.h"
#include "core_impl.h"

#include "fh_impl.h"
/* ====================================================================== */

/* This list can be accessed only with 'target_mutex' locked. */
static LIST_HEAD(fh_plugins);
/* ====================================================================== */

/* Module's init function, if set. */
static int  (*init_func)(void) = NULL;
/* Module's exit function, if set. */
static void (*exit_func)(void) = NULL;

static struct module *target_module = NULL;
/* ====================================================================== */

/* The array of pointers to function handlers structures combined from all 
 * registered FH plugins. 'handlers_size' is the number of elements in the
 * array. */
static struct kedr_fh_handlers **handlers = NULL;
static size_t handlers_size = 0;
/* ====================================================================== */

static int
plugin_handles_function(struct kedr_fh_plugin *plugin, void *func)
{
	struct kedr_fh_handlers **h;
	if (plugin->handlers == NULL)
		return 0;

	h = plugin->handlers;
	while (*h != NULL) {
		if ((*h)->orig == func)
			return 1;
		++h;
	}
	return 0;
}

/* Non-zero if some plugins process the same functions, 0 otherwise. */
static int
function_set_bad(struct kedr_fh_plugin *fh)
{
	struct kedr_fh_plugin *pos;
	struct kedr_fh_handlers **h;

	if (fh->handlers == NULL)
		return 0;

	h = fh->handlers;
	while (*h != NULL) {
		list_for_each_entry(pos, &fh_plugins, list) {
			if (plugin_handles_function(pos, (*h)->orig))
				return 1;
		}
		++h;
	}
	return 0;
}

int
kedr_fh_plugin_register_impl(struct kedr_fh_plugin *fh)
{
	struct kedr_fh_plugin *p = NULL;

	if (fh->owner == NULL)
		return -EINVAL;
	
	/* Here we can assume the session is not active (the caller is 
	 * responsible for that). */
	
	/* A sanity check first. */
	list_for_each_entry(p, &fh_plugins, list) {
		if (p == fh)
			break;
	}
	if (p == fh) {
		pr_warning(KEDR_MSG_PREFIX
	"Attempt to register a plugin that is already registered.\n");
		return -EINVAL;
	}

	/* Check if some already registered plugin handles any of the
	 * functions that 'fh' handles too. */
	if (function_set_bad(fh)) {
		pr_warning(KEDR_MSG_PREFIX
	"Attempt to register a plugin that handles some of the already "
	"handled functions.\n");
		return -EINVAL;
	}

	list_add(&fh->list, &fh_plugins);
	return 0;
}

void
kedr_fh_plugin_unregister_impl(struct kedr_fh_plugin *fh)
{
	struct kedr_fh_plugin *p = NULL;
	
	/* A sanity check first. */
	list_for_each_entry(p, &fh_plugins, list) {
		if (p == fh)
			break;
	}
	if (p != fh) {
		pr_warning(KEDR_MSG_PREFIX
	"Attempt to unregister a plugin that is not registered.\n");
		return;
	}

	list_del(&fh->list);
}

int
kedr_fh_plugins_get(void)
{
	struct kedr_fh_plugin *p = NULL;
	struct kedr_fh_plugin *to_put = NULL;
	int ret = 0;

	list_for_each_entry(p, &fh_plugins, list) {
		BUG_ON(p->owner == NULL);
		if (try_module_get(p->owner) == 0) {
			pr_err(KEDR_MSG_PREFIX
			"try_module_get() failed for the module \"%s\".\n",
			module_name(p->owner));
			ret = -ENODEV;
			break;
		}
	}

	if (ret != 0) {
		/* Unlock the modules we might have locked before the
		 * failed one. */
		list_for_each_entry(to_put, &fh_plugins, list) {
			if (to_put == p)
				break;
			module_put(to_put->owner);
		}

	}
	return ret;
}

void
kedr_fh_plugins_put(void)
{
	struct kedr_fh_plugin *p = NULL;
	list_for_each_entry(p, &fh_plugins, list) {
		BUG_ON(p->owner == NULL);
		module_put(p->owner);
	}
}
/* ====================================================================== */

/* The comparator and swapper functions needed to sort the handler table by 
 * the address of the original function. */
static int
compare_htable_items(const void *lhs, const void *rhs)
{
	const struct kedr_fh_handlers *left = 
		*(const struct kedr_fh_handlers **)lhs;
	const struct kedr_fh_handlers *right = 
		*(const struct kedr_fh_handlers **)rhs;
	
	if (left->orig < right->orig)
		return -1;
	else if (left->orig > right->orig)
		return 1;
	else 
		return 0;
}

static void
swap_htable_items(void *lhs, void *rhs, int size)
{
	struct kedr_fh_handlers **pleft = (struct kedr_fh_handlers **)lhs;
	struct kedr_fh_handlers **pright = (struct kedr_fh_handlers **)rhs;
	struct kedr_fh_handlers *t;
	
	BUG_ON(size != (int)sizeof(struct kedr_fh_handlers *));
	
	t = *pleft;
	*pleft = *pright;
	*pright = t;
}

/* If the function fails, 'handlers' will remain NULL and 'handlers_size'
 * will remain 0. */
static void
prepare_handler_table(void)
{
	struct kedr_fh_plugin *p = NULL;
	size_t sz = 0;
	struct kedr_fh_handlers **h;
	
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->handlers == NULL)
			continue;
		
		h = p->handlers;
		
		while ((*h) != NULL) {
			++h;
			++sz;
		}
	}
	
	if (sz == 0)  /* No handlers - no problems */
		return;
	
	handlers = kzalloc(sz * sizeof(*handlers), GFP_KERNEL);
	if (handlers == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
		"prepare_handler_table(): not enough memory.\n");
		return;
	}
	
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->handlers == NULL)
			continue;
		
		h = p->handlers;
		
		while ((*h) != NULL) {
			/* Sanity check */
			if (handlers_size >= sz) {
				handlers_size = 0;
				break;
			}
			
			handlers[handlers_size] = *h;
			
			++h;
			++handlers_size;
		}
		if (handlers_size != sz) {
			pr_warning(KEDR_MSG_PREFIX 
		"prepare_handler_table(): "
		"the arrays of handlers have been changed unexpectedly.\n");
			kfree(handlers);
			handlers = NULL;
			handlers_size = 0;
			return;
		}
	}
	
	sort(&handlers[0], handlers_size, sizeof(*handlers), 
		compare_htable_items, swap_htable_items);
}

static void 
destroy_handler_table(void)
{
	kfree(handlers);
	handlers_size = 0;
}
/* ====================================================================== */

void 
kedr_fh_on_session_start(struct kedr_session *session)
{
	prepare_handler_table();
}

void
kedr_fh_on_session_end(struct kedr_session *session)
{
	destroy_handler_table();
}


static void
do_call_init_pre(struct module *mod)
{
	struct kedr_fh_plugin *p = NULL;
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->on_init_pre)
			p->on_init_pre(p, mod);
	}
}

static void
do_call_init_post(struct module *mod)
{
	struct kedr_fh_plugin *p = NULL;
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->on_init_post)
			p->on_init_post(p, mod);
	}
}

static void
do_call_exit_pre(struct module *mod)
{
	struct kedr_fh_plugin *p = NULL;
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->on_exit_pre)
			p->on_exit_pre(p, mod);
	}
}

static void
do_call_exit_post(struct module *mod)
{
	struct kedr_fh_plugin *p = NULL;
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->on_exit_post)
			p->on_exit_post(p, mod);
	}
}

/* The replacement functions for the init- and exit-functions of the target
 * module. */
static int
repl_init(void)
{
	int ret = 0;

	BUG_ON(init_func == NULL);

	/* Call the original init function */
	if (init_func != NULL)
		ret = init_func();

	/* Restore the callback, the handlers might need its address. */
	target_module->init = init_func;

	do_call_init_post(target_module);

	return ret;
}

static void
repl_exit(void)
{
	BUG_ON(exit_func == NULL);

	/* Restore the callback, the handlers might need its address. */
	target_module->exit = exit_func;

	do_call_exit_pre(target_module);

	/* Call the original exit function. */
	if (exit_func != NULL)
		exit_func();
}

/* If the target module has init function, "init post" handlers will be 
 * called by our replacement for it. Otherwise, they will be called by 
 * kedr_fh_on_target_load() directly. Similar rule applies for the exit 
 * function. */
void
kedr_fh_on_target_load(struct module *mod)
{
	target_module = mod;
	init_func = target_module->init;
	
	do_call_init_pre(mod);
	
	if (init_func != NULL) {
		target_module->init = repl_init;
	}
	else {
		do_call_init_post(mod);
	}

	exit_func = target_module->exit;
	if (exit_func != NULL) {
		target_module->exit = repl_exit;
	}
}

void
kedr_fh_on_target_unload(struct module *mod)
{
	if (exit_func == NULL)
		do_call_exit_pre(mod);
	
	do_call_exit_post(mod);
	
	target_module = NULL;
}
/* ====================================================================== */

/* Looks up the handler table to find the handlers for the function with 
 * start address 'orig'. 
 * Returns the address of the handler structure if found, NULL otherwise. */
static struct kedr_fh_handlers *
lookup_handlers(unsigned long orig)
{
	size_t beg = 0;
	size_t end;
	
	end = handlers_size;
	
	/* The table must have been sorted by this time, so we can use 
	 * binary search. 
	 * At each iteration, [beg, end) range of indexes is considered. */
	while (beg < end) {
		size_t mid = beg + (end - beg) / 2;
		if (orig < (unsigned long)handlers[mid]->orig)
			end = mid;
		else if (orig > (unsigned long)handlers[mid]->orig)
			beg = mid + 1;
		else 
			return handlers[mid];
	}
	return NULL;
}

void 
kedr_fh_fill_call_info(struct kedr_call_info *info)
{
	struct kedr_fh_handlers *h;
	
	if (handlers_size == 0) {
		/* Either no handlers specified or some error occurred. */
		return;
	}
	
	h = lookup_handlers(info->target);
	if (h == NULL)
		return;
	
	if (h->pre != NULL)
		info->pre_handler = h->pre;
	
	if (h->post != NULL)
		info->post_handler = h->post;
	
	if (h->repl != NULL)
		info->repl = (unsigned long)h->repl;
}
/* ====================================================================== */
