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
#include <linux/etherdevice.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* The following happens-before relations are expressed here.
 *
 * 1. Start of register_*() HB start of each callback (net_device_ops,
 * ethtool_ops, NAPI, header_ops).
 *    ID: (ulong)&net_device.
 *
 * 2. End of each callback (see (1)) HB end of unregister_*().
 *    ID: (ulong)&net_device + 1. */
/* ====================================================================== */

/* [NB] Provided by "net_device_ops" subgroup. */
void
kedr_set_net_device_ops_handlers(const struct net_device_ops *ops);

/* [NB] Provided by "ethtool_ops" subgroup. */
void
kedr_set_ethtool_ops_handlers(const struct ethtool_ops *ops);

/* [NB] Provided by "header_ops" subgroup. */
void
kedr_set_header_ops_handlers(const struct header_ops *ops);
/* ====================================================================== */

static void
handle_register_pre(struct kedr_local_storage *ls, struct net_device *dev)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	
	/* Relation #1 */
	kedr_happens_before(ls->tid, info->pc, (unsigned long)dev);

	if (dev->netdev_ops)
		kedr_set_net_device_ops_handlers(dev->netdev_ops);

	if (dev->ethtool_ops)
		kedr_set_ethtool_ops_handlers(dev->ethtool_ops);

	if (dev->header_ops)
		kedr_set_header_ops_handlers(dev->header_ops);
}

static void
handle_unregister_post(struct kedr_local_storage *ls,
		       struct net_device *dev)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	/* Relation #2 */
	kedr_happens_after(ls->tid, info->pc, (unsigned long)dev + 1);
}
/* ====================================================================== */

/* Size of a memory block allocated for a network device with the given
 * size of private data. Same as in alloc_netdev_mqs(). */
static unsigned long
netdev_alloc_size(int sizeof_priv)
{
	unsigned long alloc_size = sizeof(struct net_device);
	if (sizeof_priv) {
		alloc_size = ALIGN(alloc_size, NETDEV_ALIGN);
		alloc_size += (unsigned long)sizeof_priv;
	}
	alloc_size += NETDEV_ALIGN - 1;
	return alloc_size;
}

static void
handle_alloc_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	int sizeof_priv = (int)KEDR_LS_ARG1(ls);
	unsigned long size;

	size = netdev_alloc_size(sizeof_priv);
	kedr_eh_on_alloc_pre(ls->tid, info->pc, size);
}

static void
handle_alloc_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	int sizeof_priv = (int)KEDR_LS_ARG1(ls);
	unsigned long addr = KEDR_LS_RET_VAL(ls);
	unsigned long size;

	size = netdev_alloc_size(sizeof_priv);
	if (addr != 0)
		kedr_eh_on_alloc_post(ls->tid, info->pc, size, addr);
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
kedr_fh_get_group_netdev(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	return &fh_group;
}
/* ====================================================================== */
