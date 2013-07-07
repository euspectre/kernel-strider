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
#include <linux/completion.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#include <util/fh_plugin.h>

#include "config.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

/* The following happens-before relation is expressed here.
 *
 * 1. Start of complete*() happens before the end of *wait_for_*() (if the 
 * latter succeeds in case of *_timeout and other variants).
 * ID: &completion. 
 */

static void
on_complete(struct kedr_local_storage *ls, void *compl)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	/* Relation #1 */
	kedr_happens_before(tid, info->pc, (unsigned long)compl);
}

static void
on_wait(struct kedr_local_storage *ls, void *compl)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	/* Relation #1 */
	kedr_happens_after(tid, info->pc, (unsigned long)compl);
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
	.handlers = NULL, /* Just to make sure all fields are zeroed. */
};

struct kedr_fh_group * __init
kedr_fh_get_group_completion(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	
	return &fh_group;
}
/* ====================================================================== */
