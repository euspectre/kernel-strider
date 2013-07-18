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

/* The following happens-before relations are expressed here.
 *
 * TODO
 */

/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

/* The common handlers for the ethtool operations. */
static void
common_pre(struct kedr_local_storage *ls)
{
	// TODO
}

static void
common_post(struct kedr_local_storage *ls)
{
	// TODO
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
