/* Handling of ethtool operations. All the callbacks currently have the
 * same handlers. */

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
#include <linux/ethtool.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* The standard happens-before relations w.r.t. register/unregister_netdev*
 * are expressed here. 
 *
 * Besides that, all ethtool_ops callbacks execute under rtnl_lock. */
/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

/* The common handlers for the ethtool operations. */
static void
common_pre(struct kedr_local_storage *ls)
{
	/* Each callback has struct net_device *dev as the first arg. */
	unsigned long dev = KEDR_LS_ARG1(ls);
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;

	kedr_rtnl_locked_start(ls, ls->fi->addr);

	/* Relation #1 */
	kedr_happens_after(tid, pc, dev);	
}

static void
common_post(struct kedr_local_storage *ls)
{
	/* Each callback has struct net_device *dev as the first arg. */
	unsigned long dev = KEDR_LS_ARG1(ls);
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	
	/* Relation #2 */
	kedr_happens_before(tid, pc, dev + 1);
	kedr_rtnl_locked_end(ls, ls->fi->addr);
}

/* Set the handlers for the ethtool operations. */
void
kedr_set_ethtool_ops_handlers(const struct ethtool_ops *ops)
{
<$set_handlers : join(\n)$>}
/* ====================================================================== */

static struct kedr_fh_group fh_group = {
	.handlers = NULL,
};

struct kedr_fh_group * __init
kedr_fh_get_group_ethtool_ops(void)
{
	/* No handlers for the functions exported by the kernel. */
	fh_group.num_handlers = 0;
	return &fh_group;
}
/* ====================================================================== */
