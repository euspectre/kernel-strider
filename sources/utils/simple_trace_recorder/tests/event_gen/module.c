/* This module generates a stream of events that can be used to test the 
 * output system. 
 * The generator provides an implementation of KernelStrider core API, so 
 * the kernel part of the output system should be built using the .symvers 
 * file of this module rather than that of kedr_mem_core.ko.
 *
 * To start the event generator, write something to 
 * "test_str_event_gen/start" in debugfs. The output system must be loaded
 * before that.
 *
 * If 'sleep_msecs' parameter is non-zero, the event generator will sleep 
 * for this number of milliseconds each time when the produced event is 
 * placed at the beginning of the new page in the output buffer. That is, 
 * after a page has been filled and writing to the next page has started, 
 * the module will sleep. This allows the user-space part of the output 
 * system to keep up and retrieve the data from the buffer. If the parameter
 * is 0, the generator will not sleep. */

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
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/debugfs.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

#include <simple_trace_recorder/recorder.h>
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[test_str_event_gen] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* How long to sleep (in milliseconds) after a page has been filled in the 
 * output buffer. 0 - do not sleep at all. */
unsigned int sleep_msecs = 2000;
module_param(sleep_msecs, uint, S_IRUGO);
/* ====================================================================== */

/* A directory for the module in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "test_str_event_gen";

/* Writing to this file will trigger the event generator. */
static struct dentry *start_file = NULL;
static const char *start_file_name = "start";
/* ====================================================================== */

/* The current set of event handlers, NULL if not specified. */
struct kedr_event_handlers *cur_eh = NULL;
/* ====================================================================== */

#define KEDR_TEST_SIGN_EXT_64(_val32) ((unsigned long)(long)(s32)(_val32))

#ifdef CONFIG_X86_64
static struct module *target = (struct module *)0xcaafbeed12345678;
static unsigned long tid1 = 0xfaad1234b00c5678;
static unsigned long tid2 = 0xea12ea34fdc1235b;
static unsigned long addr1 = 0x8eee567ad4c06bf3;
static unsigned long addr2 = 0xdeed600dfead0bf0;
static unsigned long lock1 = 0xff4856001001abcd;

#else /* x86-32 */
static struct module *target = (struct module *)0xcaafbeed;
static unsigned long tid1 = 0xb00c5678;
static unsigned long tid2 = 0xfdc1235b;
static unsigned long addr1 = 0x8eee567a;
static unsigned long addr2 = 0xdeed600d;
static unsigned long lock1 = 0x1001abcd;
#endif 

static unsigned long func1 = KEDR_TEST_SIGN_EXT_64(0xc0123ffa);
static unsigned long func2 = KEDR_TEST_SIGN_EXT_64(0xd123400b);
/* ====================================================================== */

static void
sleep_after_event(size_t event_size)
{
	static size_t sz = 0;
	if (sleep_msecs == 0)
		return;
	
	sz += event_size;
	if (sz > PAGE_SIZE) {
		/* The event did not fit into the page and has been written
		 * at the beginning of the next page. */
		sz = event_size;
		msleep(sleep_msecs);
	}
}
/* ====================================================================== */

/* An implementation of the core API, suitable for testing. Here we do not
 * care about the synchronization issues because there must be at most one
 * user of this API (the test build of the output system). Same for some of 
 * the error handling. */
int 
kedr_register_event_handlers(struct kedr_event_handlers *eh)
{
	BUG_ON(eh == NULL || eh->owner == NULL);
	
	if (cur_eh != NULL) {
		pr_err(KEDR_MSG_PREFIX 
	"Attempt to register event handlers while some handlers are "
	"already registered.\n");
		return -EINVAL;
	}
	
	cur_eh = eh;
	return 0;
}
EXPORT_SYMBOL(kedr_register_event_handlers);

void 
kedr_unregister_event_handlers(struct kedr_event_handlers *eh)
{
	BUG_ON(eh == NULL || eh->owner == NULL);
	BUG_ON(cur_eh != eh);
	
	cur_eh = NULL;
}
EXPORT_SYMBOL(kedr_unregister_event_handlers);

struct kedr_event_handlers *
kedr_get_event_handlers(void)
{
	BUG_ON(cur_eh == NULL);
	return cur_eh;
}
/* ====================================================================== */

static int
callbacks_ok(void)
{
	return (cur_eh != NULL &&
		cur_eh->on_target_loaded != NULL &&
		cur_eh->on_target_about_to_unload != NULL &&
		cur_eh->on_function_entry != NULL &&
		cur_eh->on_function_exit != NULL &&
		cur_eh->on_call_pre != NULL &&
		cur_eh->on_call_post != NULL &&
		cur_eh->begin_memory_events != NULL &&
		cur_eh->end_memory_events != NULL &&
		cur_eh->on_memory_event != NULL &&
		cur_eh->on_locked_op_post != NULL &&
		cur_eh->on_io_mem_op_post != NULL &&
		cur_eh->on_memory_barrier_pre != NULL &&
		cur_eh->on_memory_barrier_post != NULL &&
		cur_eh->on_alloc_pre != NULL &&
		cur_eh->on_alloc_post != NULL &&
		cur_eh->on_free_pre != NULL &&
		cur_eh->on_free_post != NULL &&
		cur_eh->on_lock_pre != NULL &&
		cur_eh->on_lock_post != NULL &&
		cur_eh->on_unlock_pre != NULL &&
		cur_eh->on_unlock_post != NULL &&
		cur_eh->on_signal_pre != NULL &&
		cur_eh->on_signal_post != NULL &&
		cur_eh->on_wait_pre != NULL &&
		cur_eh->on_wait_post != NULL);
	/* We ignore "thread create" and "thread join" handlers as well as
	 * the pre handlers for locked memory operations and IO memory 
	 * operations here because the output system also does so. */
}

/* Here we also don't care about synchronization. The tests themselves must
 * ensure proper order of the operations. */
static int
generate_events(void)
{
	void *data1 = NULL;
	void *data2 = NULL;
	unsigned int nr_events1 = 16;
	unsigned int nr_events_max = 32;
	unsigned int i;
	
	/* How many times to repeat certain events to make sure the amount
	 * of data is large enough (several pages or so). */
	unsigned int nr_repeat = 200;
	
	if (!callbacks_ok())
		return -EINVAL;

	cur_eh->on_target_loaded(cur_eh, target);
	sleep_after_event(sizeof(struct kedr_tr_event_module));

	cur_eh->on_function_entry(cur_eh, tid1, func1);
	sleep_after_event(sizeof(struct kedr_tr_event_func));

	cur_eh->on_call_pre(cur_eh, tid1, func1 + 0x1, func2);
	sleep_after_event(sizeof(struct kedr_tr_event_call));

	cur_eh->on_function_entry(cur_eh, tid1, func2);
	sleep_after_event(sizeof(struct kedr_tr_event_func));

	cur_eh->on_function_exit(cur_eh, tid1, func2);
	sleep_after_event(sizeof(struct kedr_tr_event_func));

	cur_eh->on_call_post(cur_eh, tid1, func1 + 0x1, func2);
	sleep_after_event(sizeof(struct kedr_tr_event_call));

	cur_eh->on_function_entry(cur_eh, tid2, func2);
	sleep_after_event(sizeof(struct kedr_tr_event_func));

	/* A block with nr_events1 possible but 0 actual events ("thread 2")
	 * and a block with 4 possible but 1 actual event ("thread 1") as
	 * if they were observed concurrently */
	cur_eh->begin_memory_events(cur_eh, tid2, nr_events1, &data2);
	cur_eh->begin_memory_events(cur_eh, tid1, 4, &data1);
	for (i = 0; i < (nr_events1 - 1); ++i) {
		cur_eh->on_memory_event(cur_eh, tid2, func2 + i, 0, 4,
			KEDR_ET_MREAD, data2);
	}
	cur_eh->on_memory_event(cur_eh, tid1, func1 + 2, 0, 4, 
			KEDR_ET_MREAD, data1);
				
	cur_eh->on_memory_event(cur_eh, tid2, func2 + (nr_events1 - 1), 0,
			4, KEDR_ET_MREAD, data2);
	cur_eh->end_memory_events(cur_eh, tid2, data2);
	
	cur_eh->on_memory_event(cur_eh, tid1, func1 + 3, 0, 4, 
			KEDR_ET_MREAD, data1);
	cur_eh->on_memory_event(cur_eh, tid1, func1 + 4, addr1, 4, 
			KEDR_ET_MREAD, data1);
	cur_eh->on_memory_event(cur_eh, tid1, func1 + 5, 0, 4, 
			KEDR_ET_MREAD, data1);
	cur_eh->end_memory_events(cur_eh, tid1, data1);
	sleep_after_event(sizeof(struct kedr_tr_event_mem));

	cur_eh->on_function_exit(cur_eh, tid2, func2);
	sleep_after_event(sizeof(struct kedr_tr_event_mem));
	
	/* A block with the maximum allowed number of the actual events. */
	data1 = NULL;
	cur_eh->begin_memory_events(cur_eh, tid1, nr_events_max, &data1);
	for (i = 0; i < nr_events_max; ++i) {
		enum kedr_memory_event_type et = KEDR_ET_MREAD;
		if (i % 3 == 0)
			et = KEDR_ET_MUPDATE;
		else if (i % 3 == 1)
			et = KEDR_ET_MWRITE;
		
		cur_eh->on_memory_event(cur_eh, tid1, func1 + 6 + i, 
			addr1 + i, 8 + 4 * i, et, data1);
	}
	cur_eh->end_memory_events(cur_eh, tid1, data1);
	sleep_after_event(sizeof(struct kedr_tr_event_mem) + 
		(nr_events_max - 1) * sizeof(struct kedr_tr_event_mem_op));
	
	/* A locked update and a locked read */
	data1 = NULL;
	if (cur_eh->on_locked_op_pre != NULL)
		cur_eh->on_locked_op_pre(cur_eh, tid1, func1 + 0x100, 
			&data1);
	cur_eh->on_locked_op_post(cur_eh, tid1, func1 + 0x100, addr2, 4,
		KEDR_ET_MUPDATE, data1);
	sleep_after_event(sizeof(struct kedr_tr_event_mem));
	
	data1 = NULL;
	if (cur_eh->on_locked_op_pre != NULL)
		cur_eh->on_locked_op_pre(cur_eh, tid1, func1 + 0x101, 
			&data1);
	cur_eh->on_locked_op_post(cur_eh, tid1, func1 + 0x101, addr2, 4,
		KEDR_ET_MREAD, data1);
	sleep_after_event(sizeof(struct kedr_tr_event_mem));
	
	/* I/O memory read and write. */
	data1 = NULL;
	if (cur_eh->on_io_mem_op_pre != NULL)
		cur_eh->on_io_mem_op_pre(cur_eh, tid1, func1 + 0x102, 
			&data1);
	cur_eh->on_io_mem_op_post(cur_eh, tid1, func1 + 0x102, addr2, 4,
		KEDR_ET_MREAD, data1);
	sleep_after_event(sizeof(struct kedr_tr_event_mem));
	
	data1 = NULL;
	if (cur_eh->on_io_mem_op_pre != NULL)
		cur_eh->on_io_mem_op_pre(cur_eh, tid1, func1 + 0x103, 
			&data1);
	cur_eh->on_io_mem_op_post(cur_eh, tid1, func1 + 0x103, addr2, 4,
		KEDR_ET_MWRITE, data1);
	sleep_after_event(sizeof(struct kedr_tr_event_mem));
	
	/* Barriers */
	cur_eh->on_memory_barrier_pre(cur_eh, tid1, func1 + 0x10, 
		KEDR_BT_FULL);
	sleep_after_event(sizeof(struct kedr_tr_event_barrier));
	cur_eh->on_memory_barrier_post(cur_eh, tid1, func1 + 0x10, 
		KEDR_BT_FULL);
	sleep_after_event(sizeof(struct kedr_tr_event_barrier));
	
	cur_eh->on_memory_barrier_pre(cur_eh, tid1, func1 + 0x20, 
		KEDR_BT_LOAD);
	sleep_after_event(sizeof(struct kedr_tr_event_barrier));
	cur_eh->on_memory_barrier_post(cur_eh, tid1, func1 + 0x20, 
		KEDR_BT_LOAD);
	sleep_after_event(sizeof(struct kedr_tr_event_barrier));
	
	cur_eh->on_memory_barrier_pre(cur_eh, tid1, func1 + 0x30, 
		KEDR_BT_STORE);
	sleep_after_event(sizeof(struct kedr_tr_event_barrier));
	cur_eh->on_memory_barrier_post(cur_eh, tid1, func1 + 0x30, 
		KEDR_BT_STORE);
	sleep_after_event(sizeof(struct kedr_tr_event_barrier));
	
	/* Alloc / free */
	/* 1. Lone "alloc pre", as if the allocation has failed */
	cur_eh->on_alloc_pre(cur_eh, tid1, func1 + 0x200, 0x1000);
	sleep_after_event(sizeof(struct kedr_tr_event_alloc_free));
	
	/* 2. Successful alloc and free */
	cur_eh->on_alloc_pre(cur_eh, tid1, func1 + 0x300, 0x100);
	sleep_after_event(sizeof(struct kedr_tr_event_alloc_free));
	cur_eh->on_alloc_post(cur_eh, tid1, func1 + 0x300, 0x100, addr2);
	sleep_after_event(sizeof(struct kedr_tr_event_alloc_free));
	
	cur_eh->on_free_pre(cur_eh, tid1, func1 + 0x300, addr2);
	sleep_after_event(sizeof(struct kedr_tr_event_alloc_free));
	cur_eh->on_free_post(cur_eh, tid1, func1 + 0x300, addr2);
	sleep_after_event(sizeof(struct kedr_tr_event_alloc_free));
	
	/* Lock / unlock */
	/* 1. Lone "lock pre", as if some kind of a trylock has failed or an
	 * interruptible lock has been interrupted. */
	cur_eh->on_lock_pre(cur_eh, tid1, func1 + 0x400, lock1, 
		KEDR_LT_MUTEX);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	/* 2. Successful locks and unlocks */
	/* Mutex */
	cur_eh->on_lock_pre(cur_eh, tid1, func1 + 0x1000, lock1, 
		KEDR_LT_MUTEX);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_lock_post(cur_eh, tid1, func1 + 0x1000, lock1, 
		KEDR_LT_MUTEX);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	cur_eh->on_unlock_pre(cur_eh, tid1, func1 + 0x1010, lock1, 
		KEDR_LT_MUTEX);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_unlock_post(cur_eh, tid1, func1 + 0x1010, lock1, 
		KEDR_LT_MUTEX);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	/* Spinlock */
	cur_eh->on_lock_pre(cur_eh, tid1, func1 + 0x2000, lock1, 
		KEDR_LT_SPINLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_lock_post(cur_eh, tid1, func1 + 0x2000, lock1, 
		KEDR_LT_SPINLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	cur_eh->on_unlock_pre(cur_eh, tid1, func1 + 0x2010, lock1, 
		KEDR_LT_SPINLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_unlock_post(cur_eh, tid1, func1 + 0x2010, lock1, 
		KEDR_LT_SPINLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	/* Read lock */
	cur_eh->on_lock_pre(cur_eh, tid1, func1 + 0x3000, lock1, 
		KEDR_LT_RLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_lock_post(cur_eh, tid1, func1 + 0x3000, lock1, 
		KEDR_LT_RLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	cur_eh->on_unlock_pre(cur_eh, tid1, func1 + 0x3010, lock1, 
		KEDR_LT_RLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_unlock_post(cur_eh, tid1, func1 + 0x3010, lock1, 
		KEDR_LT_RLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	/* Write lock */
	cur_eh->on_lock_pre(cur_eh, tid1, func1 + 0x4000, lock1, 
		KEDR_LT_WLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_lock_post(cur_eh, tid1, func1 + 0x4000, lock1, 
		KEDR_LT_WLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	cur_eh->on_unlock_pre(cur_eh, tid1, func1 + 0x4010, lock1, 
		KEDR_LT_WLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_unlock_post(cur_eh, tid1, func1 + 0x4010, lock1, 
		KEDR_LT_WLOCK);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	/* Wait and signal */
	/* 1. Successful operations. */
	cur_eh->on_wait_pre(cur_eh, tid1, func1 + 0x5000, addr1, 
		KEDR_SWT_COMMON);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_wait_post(cur_eh, tid1, func1 + 0x5000, addr1, 
		KEDR_SWT_COMMON);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	cur_eh->on_signal_pre(cur_eh, tid1, func1 + 0x5010, addr1, 
		KEDR_SWT_COMMON);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	cur_eh->on_signal_post(cur_eh, tid1, func1 + 0x5010, addr1, 
		KEDR_SWT_COMMON);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	/* 2. A lone "wait pre", as if, for example, an interruptible wait
	 * has been interrupted. */
	cur_eh->on_wait_pre(cur_eh, tid1, func1 + 0x6000, addr1, 
		KEDR_SWT_COMMON);
	sleep_after_event(sizeof(struct kedr_tr_event_sync));
	
	/* Make sure the amount of data to be transferred to the user space
	 * is at least as large as several pages. */
	for (i = 0; i < nr_repeat; ++i) {
		cur_eh->on_call_pre(cur_eh, tid1, func1 + 0x1, func2);
		sleep_after_event(sizeof(struct kedr_tr_event_call));

		cur_eh->on_function_entry(cur_eh, tid1, func2);
		sleep_after_event(sizeof(struct kedr_tr_event_func));

		cur_eh->on_function_exit(cur_eh, tid1, func2);
		sleep_after_event(sizeof(struct kedr_tr_event_func));

		cur_eh->on_call_post(cur_eh, tid1, func1 + 0x1, func2);
		sleep_after_event(sizeof(struct kedr_tr_event_call));
	}

	cur_eh->on_function_exit(cur_eh, tid1, func1);
	sleep_after_event(sizeof(struct kedr_tr_event_func));

	cur_eh->on_target_about_to_unload(cur_eh, target);
	sleep_after_event(sizeof(struct kedr_tr_event_module));
	return 0;
}
/* ====================================================================== */

static int 
start_file_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

static int
start_file_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t 
start_file_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
	int ret;
	ret = generate_events();
	if (ret != 0)
		return (ssize_t)ret;
	
	*f_pos += count; /* as if we have written something */
	return count;
}

static const struct file_operations start_file_ops = {
	.owner = THIS_MODULE,
	.open = start_file_open,
	.release = start_file_release,
	.write = start_file_write,
};
/* ====================================================================== */

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
	
	start_file = debugfs_create_file(start_file_name, 
		S_IWUSR | S_IWGRP, debugfs_dir_dentry, NULL, 
		&start_file_ops);
	if (start_file == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to create a file in debugfs (\"%s\").\n",
			start_file_name);
		ret = -ENOMEM;
		goto out_rmdir;
	}
	return 0;

out_rmdir:
	debugfs_remove(debugfs_dir_dentry);
out:
	return ret;
}

static void __exit
test_cleanup_module(void)
{
	debugfs_remove(start_file);
	debugfs_remove(debugfs_dir_dentry);
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
