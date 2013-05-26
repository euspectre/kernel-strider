/* Utilities for the FH plugins provided with KernelStrider. */

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
#include <linux/string.h>
#include <linux/slab.h>

#include <util/fh_plugin.h>

struct kedr_fh_handlers **
kedr_fh_combine_handlers(struct list_head *groups)
{
	struct kedr_fh_group *grp;
	unsigned int num = 1; /* +1 for the terminating NULL */
	unsigned int pos = 0;
	struct kedr_fh_handlers **handlers = NULL;
	
	list_for_each_entry(grp, groups, list){
		num += grp->num_handlers;
	}
	
	handlers = kzalloc(num * sizeof(handlers[0]), GFP_KERNEL);
	if (handlers == NULL)
		return NULL;
	
	list_for_each_entry(grp, groups, list){
		if (grp->num_handlers != 0) {
			memcpy(&handlers[pos], grp->handlers, 
				grp->num_handlers * sizeof(handlers[0]));
		}
		pos += grp->num_handlers;
	}
	return handlers;
}

void
kedr_fh_do_cleanup_calls(struct list_head *groups)
{
	struct kedr_fh_group *grp;
	list_for_each_entry(grp, groups, list){
		if (grp->cleanup != NULL)
			grp->cleanup();
	}
}
