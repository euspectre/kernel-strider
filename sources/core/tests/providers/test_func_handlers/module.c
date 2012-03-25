/* A module to test if the provider of the function handlers is unloadable
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

#include <kedr/kedr_mem/functions.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

static int
fake_fill(struct kedr_function_handlers *fh, struct kedr_call_info *ci)
{
	return 0;
}

static struct kedr_function_handlers test_fh = {
	.owner = THIS_MODULE,
	.fill_call_info = fake_fill,
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_set_function_handlers(NULL);
	return;
}

static int __init
test_init_module(void)
{
	kedr_set_function_handlers(&test_fh);
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
