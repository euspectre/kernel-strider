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

int called_init_pre = 0;
module_param(called_init_pre, int, S_IRUGO);

int called_init_post = 0;
module_param(called_init_post, int, S_IRUGO);

int called_exit_post = 0;
module_param(called_exit_post, int, S_IRUGO);
/* ====================================================================== */

static void
on_init_pre(struct kedr_fh_plugin *fh, struct module *mod,
	    void **per_target)
{
	if (per_target == NULL) {
		pr_warning("[test] on_init_pre: per_target is NULL.\n");
		return;
	}

	*per_target = (void *)((unsigned long)mod + 4);
	
	called_init_pre = 1;
}

static void
on_init_post(struct kedr_fh_plugin *fh, struct module *mod,
	     void **per_target)
{
	void *expected = (void *)((unsigned long)mod + 4);

	if (per_target == NULL) {
		pr_warning("[test] on_init_post: per_target is NULL.\n");
		return;
	}
	if (*per_target != expected) {
		pr_warning("[test] "
		"on_init_post: *per_target must be %p but it is %p.\n",
			expected, *per_target);
		return;
	}

	*per_target = (void *)((unsigned long)mod + 5);
	
	called_init_post = 1;	
}

static void
on_exit_post(struct kedr_fh_plugin *fh, struct module *mod,
	     void **per_target)
{
	void *expected = (void *)((unsigned long)mod + 5);

	if (per_target == NULL) {
		pr_warning("[test] on_exit_post: per_target is NULL.\n");
		return;
	}
	if (*per_target != expected) {
		pr_warning("[test] "
		"on_exit_post: *per_target must be %p but it is %p.\n",
			expected, *per_target);
		return;
	}
	
	called_exit_post = 1;	
}
/* ====================================================================== */

static struct kedr_fh_plugin fh = {
	.owner = THIS_MODULE,
	.on_init_pre = on_init_pre,
	.on_init_post = on_init_post,
	.on_exit_post = on_exit_post,
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
