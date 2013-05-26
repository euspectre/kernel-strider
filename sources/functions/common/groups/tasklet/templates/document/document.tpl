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
#include <linux/interrupt.h>

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

/* The following happens-before (HB) relations are expressed here.
 * 1. Start of a function scheduling a tasklet HB start of the tasklet.
 * ID: (ulong)tasklet_struct.
 * 2. End of a tasklet HB end of tasklet_kill().
 * ID: (ulong)tasklet_struct + 1.
 * 3. Tasklets are BH functions. 
 * IDs: as set by kedr_bh_*().
 *
 * The model is similar to that of timer API.
 *
 * [NB] Currently, we cannot handle tasklet_enable() and tasklet_disable()
 * because these functions are inline. The model of happens-before 
 * relations is incomplete here. In the future, perhaps, something will be
 * done to handle them too: annotations, patches to "uninline" them or
 * something else. */

static void
tasklet_pre(struct kedr_local_storage *ls)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;
	
	/* The callback handlers are already executed within RCU read-side
	 * section, so it is not needed to use rcu_read_lock/unlock here.
	 * rcu_dereference() IS needed, however. */
	data = rcu_dereference(ls->fi->data);
	if (data != NULL) {
		/* Relation #1 */
		unsigned long id = (unsigned long)data;
		kedr_happens_after(tid, pc, id);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX
			"tasklet_pre(): 'data' is NULL.\n");
	}
	/* Relation #3 */
	kedr_bh_start(tid, pc);
}

static void
tasklet_post(struct kedr_local_storage *ls)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;

	/* Relation #3 */
	kedr_bh_end(tid, pc);

	data = rcu_dereference(ls->fi->data);
	if (data != NULL) {
		/* Relation #2 */
		unsigned long id = (unsigned long)data + 1;
		kedr_happens_before(tid, pc, id);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX
			"tasklet_post(): 'data' is NULL.\n");
	}
}

static void
set_tasklet_handlers(struct tasklet_struct *t)
{
	struct kedr_func_info *fi;
	unsigned long flags;
	
	if (t == NULL || t->func == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"set_tasklet_handlers(): invalid tasklet.\n");
		return;
	}

	fi = kedr_find_func_info((unsigned long)t->func);
	if (fi == NULL) /* A non-instrumentable or unknown function. */
		return;

	/* If a handler is already set, it must remain unchanged.
	 * The pointer to the timer will be stored in 'data' not matter what
	 * is there. */
	spin_lock_irqsave(&fi->handler_lock, flags);
	if (fi->pre_handler == NULL)
		rcu_assign_pointer(fi->pre_handler, tasklet_pre);
	if (fi->post_handler == NULL)
		rcu_assign_pointer(fi->post_handler, tasklet_post);

	fi->data = t;
	spin_unlock_irqrestore(&fi->handler_lock, flags);
}

static void
handle_schedule(struct kedr_local_storage *ls, const char *func)
{
	struct tasklet_struct *t = 
		(struct tasklet_struct *)KEDR_LS_ARG1(ls);
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	if (t == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"%s, pre: 'tasklet_struct' is NULL.\n", func);
		return;
	}

	/* Relation #1 */
	kedr_happens_before(ls->tid, info->pc, (unsigned long)t);
	set_tasklet_handlers(t);
}

static void
handle_kill(struct kedr_local_storage *ls, const char *func)
{
	struct tasklet_struct *t = 
		(struct tasklet_struct *)KEDR_LS_ARG1(ls);
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	if (t == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"%s, post: 'tasklet_struct' is NULL.\n", func);
		return;
	}

	/* Relation #2 */
	kedr_happens_after(ls->tid, info->pc, (unsigned long)t + 1);
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
kedr_fh_get_group_tasklet(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	
	return &fh_group;
}
/* ====================================================================== */
