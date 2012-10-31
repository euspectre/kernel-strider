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

int called_kfree_pre = 0;
module_param(called_kfree_pre, int, S_IRUGO);

int called_kfree_post = 0;
module_param(called_kfree_post, int, S_IRUGO);

int called_init_pre = 0;
module_param(called_init_pre, int, S_IRUGO);

int called_exit_pre = 0;
module_param(called_exit_pre, int, S_IRUGO);

int called_exit_post = 0;
module_param(called_exit_post, int, S_IRUGO);
/* ====================================================================== */

static void
test_kfree_pre(struct kedr_local_storage *ls)
{
	(void)ls;
	called_kfree_pre = 1;
}

static void
test_kfree_post(struct kedr_local_storage *ls)
{
	(void)ls;
	called_kfree_post = 1;
}

static struct kedr_fh_handlers handlers_kfree = {
	.orig = &kfree,
	.pre = &test_kfree_pre,
	.post = &test_kfree_post,
	.repl = NULL
};

static void
test_baa_repl(const void *p)
{
	BUG();
}

static struct kedr_fh_handlers handlers_baa = {
	.orig = (void *)0x0baa1234,
	.pre = NULL,
	.post = NULL,
	.repl = &test_baa_repl
};

static struct kedr_fh_handlers *handlers[] = {
	&handlers_baa,
	&handlers_kfree,
	NULL
};
/* ====================================================================== */

static void
on_init_pre(struct kedr_fh_plugin *fh, struct module *mod)
{
	called_init_pre = 1;
}

static void
on_exit_pre(struct kedr_fh_plugin *fh, struct module *mod)
{
	called_exit_pre = 1;
}

static void
on_exit_post(struct kedr_fh_plugin *fh, struct module *mod)
{
	called_exit_post = 1;	
}
/* ====================================================================== */

static struct kedr_fh_plugin fh = {
	.owner = THIS_MODULE,
	.on_init_pre = on_init_pre,
	.on_exit_pre = on_exit_pre,
	.on_exit_post = on_exit_post,
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
