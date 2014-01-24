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

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* The following happens-before and other sync relations are expressed here.
 * 
 * 1, 2 - The standard relations for a 'poll' callback w.r.t registering and
 * unregistering of the net device. Needed because not all drivers call 
 * netif_napi_del() and besides that, we currently cannot track 
 * netif_napi_enable/disable without using annotations. Taking HB relations 
 * with register/unregister_netdev* into account allows to get rid of at 
 * least part of false positives.
 *
 * 3. Start of netif_napi_add() and start of __napi_schedule() HB start of 
 * the callback (poll()).
 *    ID: (ulong)napi.
 *
 * 4. End of the callback HB end of netif_napi_del().
 *    ID: (ulong)napi + 1.
 *
 * 5. Note that the callback is not always executed under napi->poll_lock
 * spinlock even if CONFIG_NETPOLL is set in the kernel config.
 * For example, net_rx_action (net/core/dev.c) calls napi->poll() w/o
 * napi->poll_lock. net_rx_action() is the softirq function for
 * NET_RX_SOFTIRQ. So, we imply no additional locking here.
 *
 * 6. The callback executes in BH context. */
/* ====================================================================== */

static void
poll_pre(struct kedr_local_storage *ls)
{
	struct napi_struct *napi = (struct napi_struct *)KEDR_LS_ARG1(ls);
	unsigned long func = ls->fi->addr;
	unsigned long tid = ls->tid;
	
	/* Relation #6 */
	kedr_bh_start(tid, func);

	/* Relation #1 */
	kedr_happens_after(tid, func, (unsigned long)napi->dev);
	
	/* Relation #3 */
	kedr_happens_after(tid, func, (unsigned long)napi);
}

static void
poll_post(struct kedr_local_storage *ls)
{
	struct napi_struct *napi = (struct napi_struct *)KEDR_LS_ARG1(ls);
	unsigned long func = ls->fi->addr;
	unsigned long tid = ls->tid;
	
	/* Relation #2 */
	kedr_happens_before(tid, func, (unsigned long)napi->dev + 1);
	
	/* Relation #4 */
	kedr_happens_before(tid, func, (unsigned long)napi + 1);

	/* Relation #6 */
	kedr_bh_end(tid, func);
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
kedr_fh_get_group_napi(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	return &fh_group;
}
/* ====================================================================== */
