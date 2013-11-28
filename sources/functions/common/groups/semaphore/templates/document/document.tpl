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
#include <linux/semaphore.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/fh_drd/common.h>
#include <kedr/object_types.h>

#include <util/fh_plugin.h>

#include "config.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

/* The following happens-before (HB) relation is expressed here.
 * 1. Start of up() happens-before end of down*() (if down*() is successful
 *    for the conditional variants of that function).
 * ID: (ulong)sem. */

static void
handle_up(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long sem = KEDR_LS_ARG1(ls);
	
	kedr_happens_before(ls->tid, info->pc, sem);
}

static void
handle_down(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long sem = KEDR_LS_ARG1(ls);
	
	kedr_happens_after(ls->tid, info->pc, sem);
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
kedr_fh_get_group_semaphore(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	
	return &fh_group;
}
/* ====================================================================== */
