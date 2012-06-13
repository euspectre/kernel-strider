/* This module provides API for registration/deregistration of the callbacks
 * and calls these callbacks when something is written to 
 * test_cbh_user/do_test file in debugfs. */

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
#include <linux/debugfs.h>

#include "test_cbh.h"
/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[test_cbh_user] "
/* ====================================================================== */

/* A directory for the module in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "test_cbh_user";
/* ====================================================================== */

/* A file in debugfs. Write something to it to start testing. */
static struct dentry *test_file = NULL;
static const char *test_file_name = "do_test";
/* ====================================================================== */

static struct kedr_test_cbh_ops *test_cbh_ops = NULL;

/* [NB] This module is intended for the particular test scenarios only, so
 * it can live without synchronization of the access to 'test_cbh_ops'. */
int
kedr_test_cbh_register(struct kedr_test_cbh_ops *cbh_ops)
{
	test_cbh_ops = cbh_ops;
	return 0;
}
EXPORT_SYMBOL(kedr_test_cbh_register);

void
kedr_test_cbh_unregister(struct kedr_test_cbh_ops *cbh_ops)
{
	test_cbh_ops = NULL;
}
EXPORT_SYMBOL(kedr_test_cbh_unregister);
/* ====================================================================== */

static int 
test_file_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

static int
test_file_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t 
test_file_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
	unsigned long ret; 
	/* No synchronizaton - but it is OK for testing. */
	if (test_cbh_ops != NULL && test_cbh_ops->first != NULL &&
	    test_cbh_ops->second != NULL) {
		test_cbh_ops->first();
		ret = test_cbh_ops->second(KEDR_TEST_ARG1, KEDR_TEST_ARG2, 
			KEDR_TEST_ARG3, KEDR_TEST_ARG4, KEDR_TEST_ARG5,
			KEDR_TEST_ARG6, KEDR_TEST_ARG7, KEDR_TEST_ARG8);
		
		/* Should never happen but it makes it look as if the return
		 * value were used. */
		if (ret == 0)
			return -EINVAL; 
	}
		
	*f_pos += count; /* as if we have written something */
	return count;
}

static const struct file_operations test_file_ops = {
	.owner = THIS_MODULE,
	.open = test_file_open,
	.release = test_file_release,
	.write = test_file_write,
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	debugfs_remove(test_file);
	debugfs_remove(debugfs_dir_dentry);
	return;
}

static int __init
test_init_module(void)
{
	int ret = 0;
	
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (debugfs_dir_dentry == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"Failed to create a directory in debugfs\n");
		ret = -EINVAL;
		goto out;
	}
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_warning(KEDR_MSG_PREFIX "Debugfs is not supported\n");
		ret = -ENODEV;
		goto out;
	}
	
	test_file = debugfs_create_file(test_file_name, 
		S_IWUSR | S_IWGRP, debugfs_dir_dentry, NULL, 
		&test_file_ops);
	if (test_file == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to create a file in debugfs (\"%s\").\n",
			test_file_name);
		ret = -ENOMEM;
		goto out_rmdir;
	}

out_rmdir:
	debugfs_remove(debugfs_dir_dentry);
out:
	return ret;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
