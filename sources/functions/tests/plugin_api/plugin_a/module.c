/* A function handling plugin to be used for testing. */

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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/bug.h>

#include <kedr/kedr_mem/functions.h>
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

int called_kfree = 0;
module_param(called_kfree, int, S_IRUGO);
/* ====================================================================== */

static void
test_kfree_repl(const void *p)
{
	called_kfree = 1;
	kfree(p);
}

static struct kedr_fh_handlers handlers_kfree = {
	.orig = &kfree,
	.pre = NULL,
	.post = NULL,
	.repl = &test_kfree_repl
};

static void
test_foo_repl(const void *p)
{
	BUG();
}

static struct kedr_fh_handlers handlers_foo = {
	.orig = (void *)0x0f001234,
	.pre = NULL,
	.post = NULL,
	.repl = &test_foo_repl
};

static struct kedr_fh_handlers *handlers[] = {
	&handlers_foo,
	&handlers_kfree,
	NULL
};
/* ====================================================================== */

static struct kedr_fh_plugin fh = {
	.owner = THIS_MODULE,
	.handlers = &handlers[0]
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_fh_plugin_unregister(&fh);
}

static int __init
test_init_module(void)
{
	return kedr_fh_plugin_register(&fh);
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
