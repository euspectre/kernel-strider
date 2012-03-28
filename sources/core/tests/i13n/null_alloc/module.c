/* The "NULL Allocator" for the local storage. This allocator always returns
 * NULL when a memory block for the local storage is requested from it.
 * This allows to check the fallback instances of the functions as well as
 * the relevant aspects of the function entry handling. */

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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include <kedr/kedr_mem/local_storage.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* If non-zero, more diagnostic messages will be output. */
int verbose = 0;
module_param(verbose, int, S_IRUGO);
/* ====================================================================== */

static struct kedr_local_storage *
nulla_alloc(struct kedr_ls_allocator *al)
{
	if (verbose) 
		pr_info("[kedr_null_alloc] Called alloc_ls().\n");
	
	return NULL;
}

static void 
nulla_free(struct kedr_ls_allocator *al, struct kedr_local_storage *ls)
{
	/* If everything goes as it should, this function should never
	 * be called. Anyway, nothing special to do here. */
	if (verbose) 
		pr_warning("[kedr_null_alloc] "
			"WARNING: Called free_ls() for %p.\n",
			ls);
}

struct kedr_ls_allocator null_allocator = {
	.owner = THIS_MODULE,
	.alloc_ls = nulla_alloc,
	.free_ls = nulla_free,
};
/* ====================================================================== */

static void __exit
nulla_cleanup_module(void)
{
	/* Ask the core to restore the default allocator. */
	kedr_set_ls_allocator(NULL);
	return;
}

static int __init
nulla_init_module(void)
{
	kedr_set_ls_allocator(&null_allocator);
	return 0;
}

module_init(nulla_init_module);
module_exit(nulla_cleanup_module);
/* ====================================================================== */
