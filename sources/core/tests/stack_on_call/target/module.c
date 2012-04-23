/* This module calls the functions needed for the tests. 
 * The module provides a file in debugfs (<debugfs>/<module_name>/do_test). 
 * To start the test, write something to this file.
 *
 * As the test functions to be called here are usually implemented in 
 * assembly, getting the test result can be  a little bit tricky. On 
 * success, these functions set 'kedr_test_status' global variable to 
 * a non-zero value. On failure, they leave it unchanged.
 *
 * In turn, the module reports the test result via its 'test_failed' 
 * parameter, which is 1 by default ("the tests failed or did not run"). If
 * the tests pass, this parameter will have a zero value. 
 *
 * [NB] The meddling with the file in debugfs is needed because we need to 
 * report failures via the parameter of the module, so we cannot do testing
 * in the cleanup function. To enhance the test, some kind of an event 
 * reporter is likely to be used. Symbol resolution can be very convenient 
 * when reporting the events, so we cannot perform the testing in the init
 * function either (due to the possible race on the module's symbol table and
 * other structures). 
 * For similar reasons, make sure you write to "do_test" file only after 
 * the insertion of the module into the kernel completes. */

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
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[test_stack_on_call] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Non-zero - test failed, 0 - test passed. */
int test_failed = 1;
module_param(test_failed, int, S_IRUGO);

/* By default, kedr_test_stack_on_call() is called. Set this parameter to
 * a non-zero value to make the module call kedr_test_stack_on_jmp() 
 * instead. */
int test_jmp = 0;
module_param(test_jmp, int, S_IRUGO);
/* ====================================================================== */

/* This variable should be defined in the assembly source for simplicity. 
 * Set it to 0 before calling a test function, call the function and then
 * check this variable again. 0 - test passed, non-zero - test failed. */
extern int kedr_test_status;

/* The following two functions check the correctness of the stack when 
 * processing function calls implemented using CALL and JMP instructions,
 * respectively.
 * To be exact, they check that the argument passed to a function on stack
 * remains where the function expects it even if the code has been 
 * instrumented. */
extern void kedr_test_stack_on_call(void);
extern void kedr_test_stack_on_jmp(void);
/* ====================================================================== */

/* A directory for the module in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "test_stack_on_call";
/* ====================================================================== */

/* A file in debugfs. Write something to it to start testing. */
static struct dentry *test_file = NULL;
static const char *test_file_name = "do_test";

/* A mutex to protect the test-related data: 'kedr_test_status' and other 
 * global variables the test functions might use. */
static DEFINE_MUTEX(test_mutex);
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
	if (mutex_lock_killable(&test_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "test_file_write: "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	if (test_jmp) {
		kedr_test_status = 0;
		kedr_test_stack_on_jmp();
		if (kedr_test_status == 0) {
			pr_warning(KEDR_MSG_PREFIX "test_file_write: "
				"kedr_test_stack_on_jmp() failed.\n");
			test_failed = 1; /* just in case */
			goto out;
		}
		test_failed = 0; /* pass */
	}
	else {
		kedr_test_status = 0;
		kedr_test_stack_on_call();
		if (kedr_test_status == 0) {
			pr_warning(KEDR_MSG_PREFIX "test_file_write: "
				"kedr_test_stack_on_call() failed.\n");
			test_failed = 1; /* just in case */
			goto out;
		}
		test_failed = 0; /* pass */
	}
out:	
	mutex_unlock(&test_mutex);
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
