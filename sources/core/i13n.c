/* i13n.c - the top-level component of the instrumentation subsystem. */

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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "config.h"
#include "core_impl.h"

#include "sections.h"
#include "module_ms_alloc.h"
#include "i13n.h"
/* ====================================================================== */

// TODO
/* ====================================================================== */

struct kedr_i13n *
kedr_i13n_process_module(struct module *target)
{
	struct kedr_i13n *i13n;
	int ret = 0;
	
	BUG_ON(target == NULL);
	
	i13n = kzalloc(sizeof(*i13n), GFP_KERNEL);
	if (i13n == NULL) 
		return ERR_PTR(-ENOMEM);
	
	i13n->target = target;
	INIT_LIST_HEAD(&i13n->sections);
	INIT_LIST_HEAD(&i13n->funcs);
	i13n->num_funcs = 0;
	
	i13n->detour_buffer = NULL; 
	i13n->fallback_init_area = NULL;
	i13n->fallback_core_area = NULL;
	
	ret = kedr_get_sections(target, &i13n->sections);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to obtain names and addresses of the target's sections.\n");
		goto out;
	}
	
	// TODO: initialize other stuff;
	// TODO: perform instrumentation
	return i13n;
// TODO: more cleanup in error path as needed
out:
	kfree(i13n);
	return ERR_PTR(ret);
}

void
kedr_i13n_cleanup(struct kedr_i13n *i13n)
{
	BUG_ON(i13n == NULL);
	
	// TODO: more cleanup here
	kedr_release_sections(&i13n->sections);
	kfree(i13n);
}
