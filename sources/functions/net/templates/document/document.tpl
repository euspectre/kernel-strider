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
#include <kedr/kedr_mem/local_storage.h>
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

void
kedr_locked_start(struct kedr_local_storage *ls, unsigned long pc, 
		  unsigned long lock_mask, unsigned long lock_id,
		  enum kedr_lock_type lock_type)
{
	/* Clear the bit for the lock in the status mask, just in case. */
	ls->lock_status &= ~lock_mask;

	/* Emit "lock" event only if it has not been emitted higher in the
	 * call chain. */
	if (kedr_fh_mark_locked(pc, lock_id) == 1) {
		kedr_eh_on_lock(ls->tid, pc, lock_id, lock_type);
		ls->lock_status |= lock_mask;
	}
}

void
kedr_locked_end(struct kedr_local_storage *ls, unsigned long pc, 
		unsigned long lock_mask, unsigned long lock_id,
		enum kedr_lock_type lock_type)
{
	/* Emit "unlock" event only if "lock" event has been emitted on
	 * entry to the function. */
	if (ls->lock_status & lock_mask) {
		kedr_fh_mark_unlocked(pc, lock_id);
		kedr_eh_on_unlock(ls->tid, pc, lock_id, lock_type);
		ls->lock_status &= ~lock_mask; /* just in case */
	}
}
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

	pr_info(KEDR_MSG_PREFIX "Plugin loaded.\n");
	return 0;
}

static void __exit
func_drd_exit_module(void)
{
	kedr_fh_plugin_unregister(&fh);
	kedr_fh_do_cleanup_calls(&groups);
	kfree(fh.handlers);
	
	/* [NB] If additional cleanup is needed, do it here. */

	pr_info(KEDR_MSG_PREFIX "Plugin unloaded.\n");
	return;
}

module_init(func_drd_init_module);
module_exit(func_drd_exit_module);
/* ====================================================================== */
