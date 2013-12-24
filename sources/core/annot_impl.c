/* annot_impl.c - handlers for the dynamic annotations. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>

#include "core_impl.h"
#include "config.h"
/* ====================================================================== */

/* Handlers for dynamic annotations.
 * Note that "call pre" and "call post" events are not reported for these 
 * calls, they are redundant. */

/* "happens-before" / "happens-after" */
static void 
happens_before_pre(struct kedr_local_storage *ls)
{
	/* nothing to do here */
}
static void 
happens_before_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long obj_id;
	
	/* This handler is closer to the annotated operation (the annotation
	 * is expected to be right before the latter), so we report "SIGNAL"
	 * event here rather than in the pre handler. */
	obj_id = KEDR_LS_ARG1(ls);
	kedr_happens_before(ls->tid, info->pc, obj_id);
}

static void 
happens_after_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long obj_id;
	
	/* This handler is closer to the annotated operation (the annotation
	 * is expected to be right after the latter), so we report "WAIT"
	 * event here rather than in the post handler. */
	obj_id = KEDR_LS_ARG1(ls);
	kedr_happens_after(ls->tid, info->pc, obj_id);
}

static void 
happens_after_post(struct kedr_local_storage *ls)
{
	/* nothing to do here */
}
	
/* "memory acquired" / "memory released" */
static void 
memory_acquired_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long size;
	
	size = KEDR_LS_ARG2(ls);
	if (size != 0)
		kedr_eh_on_alloc_pre(ls->tid, info->pc, size);
}
static void 
memory_acquired_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr;
	unsigned long size;
	
	addr = KEDR_LS_ARG1(ls);
	size = KEDR_LS_ARG2(ls);
	
	if (size != 0 && addr != 0)
		kedr_eh_on_alloc_post(ls->tid, info->pc, size, addr);
}

static void 
memory_released_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr;
	
	addr = KEDR_LS_ARG1(ls);
	if (addr != 0)
		kedr_eh_on_free_pre(ls->tid, info->pc, addr);
}
static void 
memory_released_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr;
	
	addr = KEDR_LS_ARG1(ls);
	if (addr != 0)
		kedr_eh_on_free_post(ls->tid, info->pc, addr);
}

struct kedr_annotation 
kedr_annotation[KEDR_ANN_NUM_TYPES] = {
	[KEDR_ANN_TYPE_HAPPENS_BEFORE]	= {
		.name = "kedr_annotate_happens_before",
		.pre  = &happens_before_pre,
		.post = &happens_before_post
	},
	[KEDR_ANN_TYPE_HAPPENS_AFTER]	= {
		.name = "kedr_annotate_happens_after",
		.pre  = &happens_after_pre,
		.post = &happens_after_post
	},
	[KEDR_ANN_TYPE_MEMORY_ACQUIRED]	= {
		.name = "kedr_annotate_memory_acquired",
		.pre  = &memory_acquired_pre,
		.post = &memory_acquired_post
	},
	[KEDR_ANN_TYPE_MEMORY_RELEASED]	= {
		.name = "kedr_annotate_memory_released",
		.pre  = &memory_released_pre,
		.post = &memory_released_post
	}
};

struct kedr_annotation *
kedr_get_annotation(enum kedr_annotation_type t)
{
	BUG_ON(t >= KEDR_ANN_NUM_TYPES);
	return &kedr_annotation[t];
}
EXPORT_SYMBOL(kedr_get_annotation);
/* ====================================================================== */
