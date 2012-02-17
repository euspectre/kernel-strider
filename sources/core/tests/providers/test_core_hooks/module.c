/* A module to test if the provider of the core hooks is unloadable
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

#include "hooks.h" 
#include "i13n.h" 

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

static void
functions_found(struct kedr_core_hooks *hooks, struct kedr_i13n *i13n)
{
	return;
}

static struct kedr_core_hooks test_hooks = {
	.owner = THIS_MODULE,
	.on_func_lookup_completed = functions_found,
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_set_core_hooks(NULL);
	return;
}

static int __init
test_init_module(void)
{
	kedr_set_core_hooks(&test_hooks);
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
