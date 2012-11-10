/* resolve_ip.c - support for obtaining information about a function given
 * an address of some location in its instrumented code. */

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
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "config.h"
#include "core_impl.h"

#include "resolve_ip.h"
#include "i13n.h"
#include "ifunc.h"
#include "target.h"
/* ====================================================================== */

/* The files in debugfs. */
static struct dentry *i_addr_file = NULL;
static struct dentry *func_name_file = NULL;
static struct dentry *func_i_start_file = NULL;

/* The name and the start address of the instrumented instance of the 
 * function to be output. 
 * [NB] kedr_for_each_loaded_target() must be called with 'session_mutex'
 * locked. This function is used to find the name and the start address of 
 * the instrumented function. So, for simplicity, we can use 'session_mutex'
 * to protect 'func_name' and 'func_i_start' too. */
static char *func_name = NULL;
static unsigned long func_i_start = 0;
/* ====================================================================== */

/* Size of the buffer to contain the string representation of the address
 * (0x...): 16 bytes maximum for the hex digits, 2 more - for "0x", the rest
 * is for the newlines or padding (if needed) and for the terminating 0. */
#define I_ADDR_BUF_SIZE 24

/* File: "i_addr", write-only */
static int 
i_addr_open(struct inode *inode, struct file *filp)
{
	filp->private_data = kzalloc(I_ADDR_BUF_SIZE, GFP_KERNEL);
	if (filp->private_data == NULL)
		return -ENOMEM;
	return nonseekable_open(inode, filp);
}

static int 
find_func(struct kedr_target *t, void *data)
{
	struct kedr_ifunc *f;
	unsigned long addr = *(unsigned long *)data;
	
	list_for_each_entry(f, &t->i13n->ifuncs, list) {
		size_t len;

		if (addr < (unsigned long)f->i_addr ||
		    addr >= (unsigned long)f->i_addr + f->i_size)
			continue;
		
		func_i_start = (unsigned long)f->i_addr;
		
		len = strlen(f->name);
		/* [NB] A newline is appended to the name to make it 
		 * look nicer when something like  "cat func_name" is 
		 * executed. */
		func_name = kzalloc(len + 2, GFP_KERNEL);
		if (func_name == NULL) {
			return -ENOMEM;
		}
		
		strncpy(func_name, f->name, len);
		func_name[len] = '\n';
		/* [NB] The terminating 0 has already been set by 
		 * kzalloc(). */
		return 1; /* found, no need to look into other targets */
	}
	
	return 0;
}

static int
i_addr_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	char *buf = filp->private_data;
	char *addr_end;
	unsigned long addr;
	
	if (mutex_lock_killable(&session_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "i_addr_release(): "
			"got a signal while trying to acquire a mutex.\n");
		ret = -EINTR;
		goto out_free;
	}
	
	kfree(func_name);
	func_name = NULL;
	func_i_start = 0;
	
	addr = simple_strtoul(buf, &addr_end, 16);
	if (addr == 0) {
		ret = -EINVAL;
		goto out;
	}
	
	ret = kedr_for_each_loaded_target(find_func, &addr);

out:	
	mutex_unlock(&session_mutex);

out_free:
	kfree(filp->private_data);
	filp->private_data = NULL;
	return ret;
}

static ssize_t 
i_addr_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
	ssize_t ret = 0;
	loff_t pos = *f_pos;
	size_t write_to;
	char *i_addr_buf = filp->private_data;
	
	if (i_addr_buf == NULL)
		return -EINVAL;
	
	if (mutex_lock_killable(&session_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "i_addr_write(): "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	if (pos < 0) {
		ret = -EINVAL;
		goto out;
	}
	
	/* 0 bytes to be written, nothing to do */
	if (count == 0)
		goto out;
	
	/* Check if the buffer has enough space for the data, including the
	 * terminating 0. */
	write_to = (size_t)pos + count;
	if (write_to + 1 >= I_ADDR_BUF_SIZE) {
		ret = -EINVAL;
		goto out;
	}
	
	if (copy_from_user(&i_addr_buf[pos], buf, count) != 0) {
		ret = -EFAULT;
		goto out;
	}
	
	mutex_unlock(&session_mutex);
	*f_pos += count;
	return count;
	
out:
	mutex_unlock(&session_mutex);
	return ret;
}

static const struct file_operations i_addr_ops = {
	.owner = THIS_MODULE,
	.open = i_addr_open,
	.release = i_addr_release,
	.write = i_addr_write,
};
/* ====================================================================== */

/* File: "func_name", read-only */
static int 
func_name_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

static int
func_name_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t 
func_name_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	ssize_t ret = 0;
	loff_t pos = *f_pos;
	size_t data_len;
	
	if (mutex_lock_killable(&session_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "func_name_read(): "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	if (func_name == NULL) {
		ret = 0; 
		goto out;
	}
	
	data_len = strlen(func_name);

	/* Reading outside of the data buffer is not allowed */
	if ((pos < 0) || (pos > data_len)) {
		ret = -EINVAL;
		goto out;
	}

	/* EOF reached or 0 bytes requested */
	if ((count == 0) || (pos == data_len)) {
		ret = 0; 
		goto out;
	}

	if (pos + count > data_len) 
		count = data_len - pos;
	if (copy_to_user(buf, &func_name[pos], count) != 0) {
		ret = -EFAULT;
		goto out;
	}

	mutex_unlock(&session_mutex);

	*f_pos += count;
	return count;

out:
	mutex_unlock(&session_mutex);
	return ret;
}

static const struct file_operations func_name_ops = {
	.owner = THIS_MODULE,
	.open = func_name_open,
	.release = func_name_release,
	.read = func_name_read,
};
/* ====================================================================== */

/* File: "func_i_start", read-only */
static int 
func_i_start_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

static int
func_i_start_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t 
func_i_start_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	ssize_t ret = 0;
	loff_t pos = *f_pos;
	size_t data_len;
	char addr_buf[I_ADDR_BUF_SIZE];
	
	if (mutex_lock_killable(&session_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "func_i_start_read(): "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	data_len = (size_t)scnprintf(&addr_buf[0], I_ADDR_BUF_SIZE, 
		"0x%lx\n", func_i_start);
	
	/* Reading outside of the data buffer is not allowed */
	if ((pos < 0) || (pos > data_len)) {
		ret = -EINVAL;
		goto out;
	}

	/* EOF reached or 0 bytes requested */
	if ((count == 0) || (pos == data_len)) {
		ret = 0; 
		goto out;
	}

	if (pos + count > data_len) 
		count = data_len - pos;
	if (copy_to_user(buf, &addr_buf[pos], count) != 0) {
		ret = -EFAULT;
		goto out;
	}

	mutex_unlock(&session_mutex);

	*f_pos += count;
	return count;

out:
	mutex_unlock(&session_mutex);
	return ret;
}

static const struct file_operations func_i_start_ops = {
	.owner = THIS_MODULE,
	.open = func_i_start_open,
	.release = func_i_start_release,
	.read = func_i_start_read,
};
/* ====================================================================== */

static void
remove_debugfs_files(void)
{
	if (i_addr_file != NULL)
		debugfs_remove(i_addr_file);
	if (func_name_file != NULL)
		debugfs_remove(func_name_file);
	if (func_i_start_file != NULL)
		debugfs_remove(func_i_start_file);
}

int 
kedr_init_resolve_ip(struct dentry *debugfs_dir)
{
	const char *name = "ERROR";
	BUG_ON(debugfs_dir == NULL);
	
	i_addr_file = debugfs_create_file("i_addr", S_IWUSR | S_IWGRP, 
		debugfs_dir, NULL, &i_addr_ops);
	if (i_addr_file == NULL) {
		name = "i_addr";
		goto out;
	}
	
	func_name_file = debugfs_create_file("func_name", S_IRUGO, 
		debugfs_dir, NULL, &func_name_ops);
	if (func_name_file == NULL) {
		name = "func_name";
		goto out;
	}
	
	func_i_start_file = debugfs_create_file("func_i_start", S_IRUGO, 
		debugfs_dir, NULL, &func_i_start_ops);
	if (func_i_start_file == NULL) {
		name = "func_i_start";
		goto out;
	}
	return 0;

out:
	pr_warning(KEDR_MSG_PREFIX 
		"Failed to create a file in debugfs (\"%s\").\n", name);
	remove_debugfs_files();
	return -ENOMEM;
}

void
kedr_cleanup_resolve_ip(void)
{
	remove_debugfs_files();
	kfree(func_name);
}
/* ====================================================================== */
