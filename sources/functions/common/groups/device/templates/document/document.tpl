/* Handling of power management callbacks in device_driver.pm is performed
 * here. All the callbacks currently have the same handlers.
 * It is possible to specify several handlers, all of them will be executed
 * (in an unspecified order). Use kedr_fh_set_pm_ops_handlers() to specify
 * the handlers. */

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
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/pm.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>

#include "config.h"
/* ====================================================================== */

/* The list of handler structures and the mutex to serialize the changes to 
 * the list. Note that the changes to the list may only happen when the 
 * target has been loaded but is not running yet. So, it is not necessary 
 * to lock the mutex from the "base" handlers. */
static LIST_HEAD(handlers);
static DEFINE_MUTEX(handlers_mutex);
/* ====================================================================== */

/* Add the handlers structure to the list if it is not there yet. */
static int
add_handlers(struct kedr_fh_drd_handlers *hs)
{
	struct kedr_fh_drd_handlers *pos;
	
	if (mutex_lock_killable(&handlers_mutex) != 0) {
		pr_warning("[kedr_fh_drd_common:device]"
			"failed to lock mutex\n");
		return -EINTR;
	}
	
	list_for_each_entry(pos, &handlers, list) {
		if (pos == hs)
			goto out;
	}
	list_add(&hs->list, &handlers);
out:
	mutex_unlock(&handlers_mutex);
	return 0;
}
/* ====================================================================== */

/* The "base" handlers. Their job is to call all other handlers for 
 * dev_pm_ops members.
 *
 * Besides that, the "base" handlers generate events to express that PM
 * operations are always executed in order and never concurrently (for a
 * given device, at least). ID: (ulong)device. */
static void
base_pre(struct kedr_local_storage *ls)
{
	struct kedr_fh_drd_handlers *pos;
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	struct device *dev = (struct device *)KEDR_LS_ARG1(ls);

	kedr_happens_after(tid, pc, (unsigned long)dev);
	
	list_for_each_entry(pos, &handlers, list) {
		if (pos->pre != NULL)
			pos->pre(ls, pos->data);
	}
}

static void
base_post(struct kedr_local_storage *ls)
{
	struct kedr_fh_drd_handlers *pos;
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	struct device *dev = (struct device *)KEDR_LS_ARG1(ls);
	
	list_for_each_entry(pos, &handlers, list) {
		if (pos->post != NULL)
			pos->post(ls, pos->data);
	}

	kedr_happens_before(tid, pc, (unsigned long)dev);
}

/* Adds the handlers to be called for dev_pm_ops callbacks.
 * 
 * If the function had already been called the given 'hs' structure,
 * it will not add another instance of 'hs' to the list.
 * 
 * The function must not be called from atomic context. */
void
kedr_set_pm_ops_handlers(struct dev_pm_ops *pm, 
	struct kedr_fh_drd_handlers *hs)
{
	if (add_handlers(hs) != 0)
		return;

<$set_handlers : join(\n)$>}
/* ====================================================================== */

static struct kedr_fh_group fh_group = {
	.handlers = NULL, /* Just to make sure all fields are zeroed. */
};

struct kedr_fh_group * __init
kedr_fh_get_group_device(void)
{
	/* This group provides no handlers for exported functions. */
	fh_group.num_handlers = 0;
	return &fh_group;
}
/* ====================================================================== */
