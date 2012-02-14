/* A module to test if the provider of a custom allocator is unloadable
 * while the target is in memory. */

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
#include <linux/init.h>
#include <linux/kernel.h>

#include <kedr/kedr_mem/local_storage.h> 

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* "NULL Allocator", a custom allocator that always fails. */
static struct kedr_local_storage *
null_alloc_ls(struct kedr_ls_allocator *al)
{
	return NULL;
}

static void 
null_free_ls(struct kedr_ls_allocator *al, 
	struct kedr_local_storage *ls)
{
	return;
}

static struct kedr_ls_allocator null_allocator = {
	.owner = THIS_MODULE,
	.alloc_ls = null_alloc_ls,
	.free_ls  = null_free_ls,
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_set_ls_allocator(NULL);
	return;
}

static int __init
test_init_module(void)
{
	kedr_set_ls_allocator(&null_allocator);
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
