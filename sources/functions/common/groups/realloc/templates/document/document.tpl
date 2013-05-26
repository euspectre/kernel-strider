/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin
 *      Andrey V. Tsyvarev
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gfp.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#include <util/fh_plugin.h>

#include "config.h"
/* ====================================================================== */

static void 
func_drd___krealloc_pre(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long size;
		
	eh = kedr_get_event_handlers();
	
	/* Even if no memory allocation actually happens, report 
	 * "alloc pre" event anyway. */
	if (eh->on_alloc_pre != NULL) {
		size = KEDR_LS_ARG2(ls);
		if (size != 0)
			eh->on_alloc_pre(eh, ls->tid, info->pc, size);
	}
}

static void 
func_drd___krealloc_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long p;
	unsigned long size;
	unsigned long addr;
	
	eh = kedr_get_event_handlers();
	p = KEDR_LS_ARG1(ls);
	size = KEDR_LS_ARG2(ls);
	addr = KEDR_LS_RET_VAL(ls);
	
	if (size == 0)
		return;
	
	if (p == 0) { 
		/* same as kmalloc */
		if (addr != 0 && eh->on_alloc_post != NULL)
			eh->on_alloc_post(eh, ls->tid, info->pc, size, 
				addr);
	} 
	else {
		/* this part is more tricky as __krealloc may or may not call
		 * kmalloc and, in addition, it does not free 'p'. */
		if (addr != 0 && addr != p && eh->on_alloc_post != NULL) 
			/* allocation has been done, report it */
			eh->on_alloc_post(eh, ls->tid, info->pc, size, 
				addr);
	}
}
/* ====================================================================== */

/* [NB] No need to register memory access made by krealloc() when copying
 * the data. If there is a race between that and some other access, we 
 * should be able to detect it as a race between usage and free.
 *
 * Note that a post-handler for a call to krealloc may actually be called
 * after the post handler for some other function even if krealloc was 
 * called before that function. Similarly, to what is done in KEDR 
 * (LeakCheck), we record 'free' operation first to avoid problems due
 * to such violation of "happens-before" relation. */
static void 
func_drd_krealloc_pre(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long p;
	unsigned long size;
		
	eh = kedr_get_event_handlers();
	
	p = KEDR_LS_ARG1(ls);
	size = KEDR_LS_ARG2(ls);
	
	if (size == 0) {
		/* Case 1: "free". */
		if (!ZERO_OR_NULL_PTR((void *)p) && 
			eh->on_free_pre != NULL)
			eh->on_free_pre(eh, ls->tid, info->pc, p); 
	}
	else if (p == 0) {
		/* Case 2: "alloc". */
		if (eh->on_alloc_pre != NULL)
			eh->on_alloc_pre(eh, ls->tid, info->pc, size);
	}
	else {
		/* Case 3: modelled as "free"+"alloc" despite the
		 * operations actually go in the reverse order. */
		if (!ZERO_OR_NULL_PTR((void *)p) && 
			eh->on_free_pre != NULL)
			eh->on_free_pre(eh, ls->tid, info->pc, p); 
	}
}

static void 
func_drd_krealloc_post(struct kedr_local_storage *ls)
{
	struct kedr_event_handlers *eh;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long p;
	unsigned long size;
	unsigned long addr;
	
	eh = kedr_get_event_handlers();
	p = KEDR_LS_ARG1(ls);
	size = KEDR_LS_ARG2(ls);
	addr = KEDR_LS_RET_VAL(ls);
	
	if (size == 0) {
		/* Case 1: "free". */
		if (!ZERO_OR_NULL_PTR((void *)p) && 
			eh->on_free_post != NULL) {
			eh->on_free_post(eh, ls->tid, info->pc, p); 
		}
	}
	else if (p == 0) {
		/* Case 2: "alloc". */
		if (addr != 0 && eh->on_alloc_post != NULL)
			eh->on_alloc_post(eh, ls->tid, info->pc, size,
				addr);
	}
	else {
		/* Now to the tricky part. Report "free post" and then 
		 * report an allocation. If the actual allocation has
		 * failed, report a fake allocation with 'p' as a 
		 * result. */
		if (!ZERO_OR_NULL_PTR((void *)p) && 
			eh->on_free_post != NULL) {
			eh->on_free_post(eh, ls->tid, info->pc, p); 
		}
		
		if (eh->on_alloc_pre != NULL) {
			eh->on_alloc_pre(eh, ls->tid, info->pc, size);
		}
		
		if (eh->on_alloc_post != NULL) {
			eh->on_alloc_post(eh, ls->tid, info->pc, size,
				(addr != 0 ? addr : p));
		}
	}
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
kedr_fh_get_group_realloc(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	
	return &fh_group;
}
/* ====================================================================== */

