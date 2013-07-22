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
#include <linux/rtnetlink.h>
#include <net/rtnetlink.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* This group provides the API to handle rtnl-locked callbacks and processes
 * rtnl_lock/unlock/trylock functions. 
 *
 * Besides that, the following happens-before relations and locking events
 * for rtnl_link_ops are expressed here:
 *
 * 1. Start of *rtnl_link_register() HB start of each callback.
 *    ID: (ulong)ops.
 *
 * 2. End of each callback HB *rtnl_link_unregister().
 *    ID: (ulong)ops + 1.
 *
 * 3. Some of the callbacks execute under rtnl_lock. */
/* ====================================================================== */

/* As the mutex used in rntl_lock() is not available for the drivers
 * directly, we provide our own ID for it (to be used in kedr_eh_on_lock(),
 * etc.).
 * We use an address of a variable for this purpose to make sure the ID
 * never conflicts with other IDs, which are addresses of some objects too.
 */
static unsigned long rtnl_lock_stub;
static unsigned long rtnl_lock_id = (unsigned long)&rtnl_lock_stub;

static void
on_rtnl_lock_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	kedr_eh_on_lock_pre(ls->tid, info->pc, rtnl_lock_id, KEDR_LT_MUTEX);
}

static void
on_rtnl_lock_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	if (kedr_fh_mark_locked(info->pc, rtnl_lock_id) == 1)
		kedr_eh_on_lock_post(ls->tid, info->pc, rtnl_lock_id,
				     KEDR_LT_MUTEX);
}

static void
on_rtnl_unlock_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	kedr_fh_mark_unlocked(info->pc, rtnl_lock_id);
	kedr_eh_on_unlock_pre(ls->tid, info->pc, rtnl_lock_id,
			      KEDR_LT_MUTEX);
}

static void
on_rtnl_unlock_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	kedr_eh_on_unlock_post(ls->tid, info->pc, rtnl_lock_id,
			       KEDR_LT_MUTEX);
}

void
kedr_rtnl_locked_start(struct kedr_local_storage *ls, unsigned long pc)
{
	kedr_locked_start(ls, pc, KEDR_LOCK_MASK_RTNL, rtnl_lock_id, 
			  KEDR_LT_MUTEX);
}

void
kedr_rtnl_locked_end(struct kedr_local_storage *ls, unsigned long pc)
{
	kedr_locked_end(ls, pc, KEDR_LOCK_MASK_RTNL, rtnl_lock_id, 
			  KEDR_LT_MUTEX);
}
/* ====================================================================== */

/* [NB] Provided by rtnl_link_ops subgroup. */
void
kedr_set_rtnl_link_ops_handlers(const struct rtnl_link_ops *ops);
/* ====================================================================== */

static void
handle_register_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	struct rtnl_link_ops *ops =
		(struct rtnl_link_ops *)KEDR_LS_ARG1(ls);

	if (ops == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"rtnl: handle_register_pre(): ops is NULL.\n");
		return;
	}
		
	/* Relation #1 */
	kedr_happens_before(ls->tid, info->pc, (unsigned long)ops);
	kedr_set_rtnl_link_ops_handlers(ops);
}

static void
handle_unregister_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	struct rtnl_link_ops *ops =
		(struct rtnl_link_ops *)KEDR_LS_ARG1(ls);

	if (ops == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"rtnl: handle_unregister_post(): ops is NULL.\n");
		return;
	}

	/* Relation #2 */
	kedr_happens_after(ls->tid, info->pc, (unsigned long)ops + 1);
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
	.handlers = NULL,
};

struct kedr_fh_group * __init
kedr_fh_get_group_rtnl(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	return &fh_group;
}
/* ====================================================================== */
