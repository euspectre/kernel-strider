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
#include <linux/mutex.h>
#include <linux/list.h>

#include <kedr/kedr_mem/functions.h>

#include "config.h"
#include "core_impl.h"

#include "fh_impl.h"
/* ====================================================================== */

/* This list can be accessed only with 'session_mutex' locked. */
static LIST_HEAD(fh_plugins);

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

/* Returns the number of currently registered FH plugins. Can be called
 * only if the session is active or 'session_mutex' is locked, i.e., the
 * list of the plugins does not change. */
size_t
fh_plugins_count(void)
{
	size_t s = 0;
	struct kedr_fh_plugin *p;
	
	list_for_each_entry(p, &fh_plugins, list)
		++s;

	return s;
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
	int is_err = 0;
	
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
				is_err = 1;
				break;
			}
			handlers[handlers_size] = *h;
			++h;
			++handlers_size;
		}
		if (is_err)
			break;
	}
	if (is_err || handlers_size != sz) {
		pr_warning(KEDR_MSG_PREFIX 
	"prepare_handler_table(): "
	"the arrays of handlers have been changed unexpectedly.\n");
		kfree(handlers);
		handlers = NULL;
		handlers_size = 0;
		return;
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

/* Per-target data. Should only be created and used when a session is
 * active. */
struct kedr_per_target
{
	struct list_head list;

	/* Target module. */
	struct module *mod;

	/* The array of per-target data blocks, one block for each
	 * registered plugin in the same order as the plugins are in
	 * 'fh_plugins'. */
	void *data[1];
};

static LIST_HEAD(per_target_items);

/* A mutex to serialize accesses to 'per_target_items'. */
static DEFINE_MUTEX(per_target_mutex);
/* ====================================================================== */

/* 'per_target_*()' functions may be called from on_init / on_exit handlers
 * only. */

/* Must be used with 'per_target_mutex' locked.
 * It seems enough to use a plain linear search here when looking for an ID
 * for the module as on_init and on_exit callbacks should not be called very
 * often. */
static struct kedr_per_target *
per_target_find_impl(struct module *mod)
{
	struct kedr_per_target *pt;
	list_for_each_entry(pt, &per_target_items, list) {
		if (pt->mod == mod)
			return pt;
	}

	return NULL;
}

/* Returns the per-target structure for a given target module 'mod' or NULL
 * if not found. */
static struct kedr_per_target *
per_target_find(struct module *mod)
{
	struct kedr_per_target *pt;
	
	if (mutex_lock_killable(&per_target_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX
			"per_target_create(): failed to lock mutex.\n");
		return NULL;
	}

	pt = per_target_find_impl(mod);
	
	mutex_unlock(&per_target_mutex);
	return pt;
}

/* Creates a per-target structure for a given module. Returns the structure
 * if successful, NULL otherwise.
 * NULL is also returned if there are no FH plugins (no need for such data
 * in this case). */
static struct kedr_per_target *
per_target_create(struct module *mod)
{
	struct kedr_per_target *pt = NULL;
	size_t s = 0;
	size_t plugin_count;

	if (mutex_lock_killable(&per_target_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX
			"per_target_create(): failed to lock mutex.\n");
		return NULL;
	}

	plugin_count = fh_plugins_count();
	if (plugin_count == 0)
		goto out;
	
	if (per_target_find_impl(mod) != NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"per_target_create(): per-target data for %s "
			"already exists.\n", module_name(mod));
		goto out;
	}
	
	s = sizeof(*pt) + (plugin_count - 1) * sizeof(pt->data[0]);
	pt = kzalloc(s, GFP_KERNEL);
	if (pt == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"per_target_create(): out of memory.\n");
		goto out;
	}

	pt->mod = mod;
	list_add(&pt->list, &per_target_items);

out:
	mutex_unlock(&per_target_mutex);
	return pt;
}

/* Destroys the given per-target structure. */
static void
per_target_destroy(struct kedr_per_target *pt)
{
	if (mutex_lock_killable(&per_target_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX
			"per_target_destroy(): failed to lock mutex\n");
		return;
	}

	list_del(&pt->list);
	kfree(pt);

	mutex_unlock(&per_target_mutex);
}
/* ====================================================================== */

void 
kedr_fh_on_session_start(void)
{
	prepare_handler_table();
}

void
kedr_fh_on_session_end(void)
{
	destroy_handler_table();

	if (!list_empty(&per_target_items)) {
		struct kedr_per_target *pt;
		struct kedr_per_target *tmp;
		
		WARN_ON(1);
		
		/* Cleanup anyway */
		list_for_each_entry_safe(pt, tmp, &per_target_items, list)
			per_target_destroy(pt);
	}
}

static void
do_call_init_pre(struct module *mod)
{
	struct kedr_fh_plugin *p = NULL;
	struct kedr_per_target *pt = NULL;
	size_t index = 0;

	if (list_empty(&fh_plugins))
		return;

	pt = per_target_create(mod);
	if (pt == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"on_init_pre() callbacks will not be called.\n");
		return;
	}
	
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->on_init_pre)
			p->on_init_pre(p, mod, &pt->data[index]);

		++index;
	}
}

static void
do_call_init_post(struct module *mod)
{
	struct kedr_fh_plugin *p = NULL;
	struct kedr_per_target *pt = NULL;
	size_t index = 0;

	if (list_empty(&fh_plugins))
		return;

	pt = per_target_find(mod);
	if (pt == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"No per-target data for module %s. "
			"on_init_post() callbacks will not be called.\n",
			module_name(mod));
		return;
	}
	
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->on_init_post)
			p->on_init_post(p, mod, &pt->data[index]);
		++index;
	}
}

static void
do_call_exit_pre(struct module *mod)
{
	struct kedr_fh_plugin *p = NULL;
	struct kedr_per_target *pt = NULL;
	size_t index = 0;

	if (list_empty(&fh_plugins))
		return;

	pt = per_target_find(mod);
	if (pt == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"No per-target data for module %s. "
			"on_exit_pre() callbacks will not be called.\n",
			module_name(mod));
		return;
	}
	
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->on_exit_pre)
			p->on_exit_pre(p, mod, &pt->data[index]);
		++index;
	}
}

static void
do_call_exit_post(struct module *mod)
{
	struct kedr_fh_plugin *p = NULL;
	struct kedr_per_target *pt = NULL;
	size_t index = 0;

	if (list_empty(&fh_plugins))
		return;

	pt = per_target_find(mod);
	if (pt == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"No per-target data for module %s. "
			"on_exit_post() callbacks will not be called.\n",
			module_name(mod));
		return;
	}
	
	list_for_each_entry(p, &fh_plugins, list) {
		if (p->on_exit_post)
			p->on_exit_post(p, mod, &pt->data[index]);
		++index;
	}

	per_target_destroy(pt);
}

/* If the target module has init function, "init post" handlers will be 
 * called for it. Otherwise, they will not be called. 
 * Similar rule applies to the handlers for the exit function. */
void
kedr_fh_on_target_load(struct module *mod)
{
	do_call_init_pre(mod);
}

void
kedr_fh_on_target_unload(struct module *mod)
{
	do_call_exit_post(mod);
}

void
kedr_fh_on_init_post(struct module *mod)
{
	do_call_init_post(mod);
}

void
kedr_fh_on_exit_pre(struct module *mod)
{
	do_call_exit_pre(mod);
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
