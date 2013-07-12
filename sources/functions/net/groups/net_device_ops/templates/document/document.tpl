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

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* The following happens-before relations are expressed here.
 *
 * TODO
 */

/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

static struct kedr_fh_group fh_group = {
	.handlers = NULL,
};
/* ====================================================================== */

/* Set the handlers for the net_device_ops callbacks. */
void
kedr_set_net_device_ops_handlers(const struct net_device_ops *ops)
{
<$set_handlers : join(\n)$>}
/* ====================================================================== */

struct kedr_fh_group * __init
kedr_fh_get_group_net_device_ops(void)
{
	/* No handlers of the exported functions, there are only handlers
	 * for the callbacks here. */
	fh_group.num_handlers = 0;
	return &fh_group;
}
/* ====================================================================== */
