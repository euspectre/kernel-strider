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
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/neighbour.h>

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
 * are expressed here. */
/* ====================================================================== */

/* Common pre and post handlers. */
static void
handle_pre_common(struct kedr_local_storage *ls, struct net_device *dev)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	
	/* Relation #1 */
	kedr_happens_after(tid, pc, (unsigned long)dev);
}

static void
handle_post_common(struct kedr_local_storage *ls, struct net_device *dev)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	
	/* Relation #2 */
	kedr_happens_before(tid, pc, (unsigned long)dev + 1);
}
/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

static struct kedr_fh_group fh_group = {
	.handlers = NULL,
};
/* ====================================================================== */

/* Set the handlers for the header_ops callbacks. */
void
kedr_set_header_ops_handlers(const struct header_ops *ops)
{
<$set_handlers : join(\n)$>}
/* ====================================================================== */

struct kedr_fh_group * __init
kedr_fh_get_group_header_ops(void)
{
	/* No handlers of the exported functions, there are only handlers
	 * for the callbacks here. */
	fh_group.num_handlers = 0;
	return &fh_group;
}
/* ====================================================================== */
