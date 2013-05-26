/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Author: 
 *      Eugene A. Shatokhin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/timer.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/fh_drd/common.h>
#include <kedr/object_types.h>

#include <util/fh_plugin.h>

#include "config.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

/* The following happens-before relations are expressed here:
 * - timer function starts executing only after it has been (re)registered;
 * - timer function finished strictly before *del_timer_sync() functions
 * return.
 *
 * ID for the 1st relation is the address of the relevant struct timer_list.
 * ID for the 2nd one is the address of struct timer_list + 1. */

static void
timer_function_pre(struct kedr_local_storage *ls)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;
	
	/* The callback handlers are already executed within RCU read-side
	 * section, so it is not needed to use rcu_read_lock/unlock here.
	 * rcu_dereference() IS needed, however. */
	data = rcu_dereference(ls->fi->data);
	if (data != NULL) {
		unsigned long id = (unsigned long)data;
		kedr_happens_after(tid, pc, id);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX
			"timer_function_pre(): 'data' is NULL.\n");
	}
	kedr_bh_start(tid, pc);
}

static void
timer_function_post(struct kedr_local_storage *ls)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;

	kedr_bh_end(tid, pc);

	data = rcu_dereference(ls->fi->data);
	if (data != NULL) {
		unsigned long id = (unsigned long)data + 1;
		kedr_happens_before(tid, pc, id);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX
			"timer_function_post(): 'data' is NULL.\n");
	}
}

static void
set_timer_function_handlers(struct timer_list *t)
{
	struct kedr_func_info *fi;
	unsigned long flags;
	
	if (t == NULL || t->function == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"set_timer_function_handlers(): invalid timer.\n");
		return;
	}

	fi = kedr_find_func_info((unsigned long)t->function);
	if (fi == NULL) /* A non-instrumentable or unknown function. */
		return;

	/* If a handler is already set, it must remain unchanged.
	 * The pointer to the timer will be stored in 'data' not matter what
	 * is there. */
	spin_lock_irqsave(&fi->handler_lock, flags);
	if (fi->pre_handler == NULL)
		rcu_assign_pointer(fi->pre_handler, timer_function_pre);
	if (fi->post_handler == NULL)
		rcu_assign_pointer(fi->post_handler, timer_function_post);

	fi->data = t;
	spin_unlock_irqrestore(&fi->handler_lock, flags);
}

static void
handle_add_mod_timer(struct kedr_local_storage *ls, const char *func)
{
	struct timer_list *timer = (struct timer_list *)KEDR_LS_ARG1(ls);
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	if (timer == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"%s, pre: 'timer' is NULL.\n", func);
		return;
	}

	kedr_happens_before(ls->tid, info->pc, (unsigned long)timer);
	set_timer_function_handlers(timer);
}

static void
handle_del_timer_sync(struct kedr_local_storage *ls, const char *func)
{
	struct timer_list *timer = (struct timer_list *)KEDR_LS_ARG1(ls);
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	if (timer == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"%s, post: 'timer' is NULL.\n", func);
		return;
	}

	kedr_happens_after(ls->tid, info->pc, (unsigned long)timer + 1);
}
/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

static struct kedr_fh_handlers *handlers[] = {
	<$if concat(handlerItem)$><$handlerItem: join(,\n\t)$>,
	<$endif$>NULL
};
/* ====================================================================== */

static struct kedr_fh_group fh_group = {
	.handlers = NULL, /* Just to make sure all fields are zeroed. */
};

struct kedr_fh_group * __init
kedr_fh_get_group_timer(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	
	return &fh_group;
}
/* ====================================================================== */
