/* This module handles the operations (calls of kernel API functions,
 * execution of callback functions) specific to the network drivers and
 * dumps the information for data race detection to the trace. */
	
/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Author: 
 *      Eugene A. Shatokhin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* The list of function groups. */
static LIST_HEAD(groups);
/* ====================================================================== */

<$declare_group : join(\n)$>
/* ====================================================================== */

static struct kedr_fh_plugin fh = {
	.owner = THIS_MODULE,
};
/* ====================================================================== */

static int __init
func_drd_init_module(void)
{
	int ret;
	
	/* Add the groups of functions to be handled. */
<$add_group : join(\n)$>
	
	fh.handlers = kedr_fh_combine_handlers(&groups);
	if (fh.handlers == NULL)
		return -ENOMEM;
	
	ret = kedr_fh_plugin_register(&fh);
	if (ret != 0) {
		kfree(fh.handlers);
		return ret;
	}

	pr_info(KEDR_MSG_PREFIX "Plugin loaded.");
	return 0;
}

static void __exit
func_drd_exit_module(void)
{
	kedr_fh_plugin_unregister(&fh);
	kedr_fh_do_cleanup_calls(&groups);
	kfree(fh.handlers);
	
	/* [NB] If additional cleanup is needed, do it here. */

	pr_info(KEDR_MSG_PREFIX "Plugin unloaded.");
	return;
}

module_init(func_drd_init_module);
module_exit(func_drd_exit_module);
/* ====================================================================== */
