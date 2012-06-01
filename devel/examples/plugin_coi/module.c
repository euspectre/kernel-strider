/* A plugin to the function handling subsystem that allows to use KEDR-COI
 * to establish several kinds of happens-before links. */

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
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <kedr/kedr_mem/functions.h>
#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_drd_plugin_coi] "
/* ====================================================================== */

static void 
on_before_exit(struct module *mod)
{
	// TODO
	//<>
	pr_info(KEDR_MSG_PREFIX "Called on_before_exit() for \"%s\".\n",
		module_name(mod));
	//<>
}
/* ====================================================================== */

static int 
repl_cdev_add(struct cdev *p, dev_t dev, unsigned count)
{
	int ret;
	
	//<>
	pr_info(KEDR_MSG_PREFIX "cdev_add: pre\n");
	//<>
	
	ret = cdev_add(p, dev, count);
	
	//<>
	pr_info(KEDR_MSG_PREFIX "cdev_add: post\n");
	//<>
	
	return ret;
}

static void 
repl_cdev_del(struct cdev *p)
{
	//<>
	pr_info(KEDR_MSG_PREFIX "cdev_del: pre\n");
	//<>
	
	cdev_del(p);
	
	//<>
	pr_info(KEDR_MSG_PREFIX "cdev_del: post\n");
	//<>
}

struct kedr_repl_pair rp[] = {
	{&cdev_add, &repl_cdev_add},
	{&cdev_del, &repl_cdev_del},
	/* [NB] Add more replacement functions if needed */
	{NULL, NULL}
};
/* ====================================================================== */

struct kedr_fh_plugin fh_plugin = {
	.owner = THIS_MODULE,
	.on_before_exit_call = on_before_exit,
	.repl_pairs = rp
};
/* ====================================================================== */

static void __exit
plugin_coi_exit(void)
{
	kedr_fh_plugin_unregister(&fh_plugin);
	
	// TODO: more cleanup if needed
	return;
}

static int __init
plugin_coi_init(void)
{
	int ret = 0;
	
	// TODO: more initalization if needed
	
	ret = kedr_fh_plugin_register(&fh_plugin);
	if (ret != 0)
		goto out;
	return 0;
out:
	return ret;
}

module_init(plugin_coi_init);
module_exit(plugin_coi_exit);
/* ====================================================================== */
