/* Handling of the callbacks from struct rtnl_link_ops is performed here. */

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
#include <net/rtnetlink.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/fh_drd/common.h>

#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* Common pre and post handlers. */
static void
handle_pre_common(struct kedr_local_storage *ls)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;

	data = rcu_dereference(ls->fi->data);
	if (data == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"rtnl_link_ops: handle_pre_common(): data is NULL.\n");
		return;
	}

	/* Relation #1 */
	kedr_happens_after(tid, pc, (unsigned long)data);
}

static void
handle_post_common(struct kedr_local_storage *ls)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;

	data = rcu_dereference(ls->fi->data);
	if (data == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"rtnl_link_ops: handle_post_common(): data is NULL.\n");
		return;
	}

	/* Relation #2 */
	kedr_happens_before(tid, pc, (unsigned long)data + 1);
}

/* Pre and post handlers for the callbacks that execute under rtnl_lock. */
static void
handle_pre_locked(struct kedr_local_storage *ls)
{
	kedr_rtnl_locked_start(ls, ls->fi->addr);
	handle_pre_common(ls);
}

static void
handle_post_locked(struct kedr_local_storage *ls)
{
	handle_post_common(ls);
	kedr_rtnl_locked_end(ls, ls->fi->addr);
}
/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

void 
kedr_set_rtnl_link_ops_handlers(const struct rtnl_link_ops *ops)
{
<$set_handlers : join(\n)$>}
/* ====================================================================== */
