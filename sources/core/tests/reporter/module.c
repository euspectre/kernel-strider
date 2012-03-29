/* This module saves the information about the events it receives from the 
 * core to a file in debugfs. The parameters of the module control which
 * types of events to report this way.
 * 
 * The module can operate in two modes, depending on the value of 
 * 'target_function' parameter:
 * - if the parameter has an empty value, all events allowed by "report_*"
 * parameters will be reported;
 * - if the parameter has a non-empty value (name of the function), only the
 * events starting from the first entry to the function and up to the exit
 * from that function in the same thread will be reported (and only the 
 * events from that thread will be reported) if enabled by "report_*".
 * 
 * [NB] In the second mode, the module cannot handle the targets where that
 * function is called recursively (the reporter must not crash but the 
 * report itself is likely to contain less data than expected).
 * 
 * Format of the output records is as follows (the leading spaces are only 
 * for readability).
 * - Format of the records for function entry events:
 *	TID=<tid,0x%lx> FENTRY name="<name of the function>"
 * - Format of the records for function exit events:
 *	TID=<tid,0x%lx> FEXIT name="<name of the function>"
 * - Format of the records for "call pre" events:
 *	TID=<tid,0x%lx> CALL_PRE pc=<pc,%pS> name="<name of the callee>"
 * - Format of the records for "call post" events:
 *	TID=<tid,0x%lx> CALL_POST pc=<pc,%pS> name="<name of the callee>"
 *
 * [NB] If a function to be mentioned in the report is in "init" area of the
 * target module, its name may sometimes be resolved incorrectly (usually,
 * to an empty string). */

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
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/kallsyms.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

#include "debug_util.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_test_reporter] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* The name of the function to report the events for. The events from the 
 * function itself and from the functions it calls (to ) */
char *target_function = "";
module_param(target_function, charp, S_IRUGO);

/* The maximum number of events that can be reported in a single session
 * (from loading to unloading of the target or from the function entry to 
 * the function exit). After this number of events has been reported, the 
 * following events in this session will be skipped. 
 * The greater this parameter is, the more memory the module needs to 
 * contain the report. */
unsigned int max_events = 65536;
module_param(max_events, uint, S_IRUGO);

/* If non-zero, call pre/post and function entry/exit events will be 
 * reported. */
int report_calls = 0;
module_param(report_calls, int, S_IRUGO);
/* ====================================================================== */

/* A directory for the module in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "kedr_test_reporter";
/* ====================================================================== */

/* A single-threaded (ordered) workqueue where the requests to handle the 
 * events are placed. The requests are guaranteed to be serviced strictly 
 * one-by-one, in FIFO order. 
 *
 * When the target has executed its cleanup function and is about to unload,
 * the workqueue should be flushed and our on_target_unload() handler would 
 * therefore wait for all pending requests to be processed. */
static struct workqueue_struct *wq = NULL; 

/* The name of the workqueue. */
static const char *wq_name = "kedr_rp_wq";

/* A spinlock that protects the top half of event handling, that is, adding
 * elements to the workqueue. The bottom half (processing these elements) is
 * taken care of by the workqueue itself. The workqueue is ordered, so no
 * additional synchronization is needed there. */
static DEFINE_SPINLOCK(wq_lock);
/* ====================================================================== */

/* This flag specifies if the events should be reported. */
static int within_target_func = 0;

/* Restrict the reported events to a span from the first entry to the given
 * function up to the exit from that function in the same thread. In 
 * addition, if this flag is non-zero, only the events from the same thread
 * as for that function entry will be reported.
 * If this flag is 0, no such restrictions are imposed. That is, if 
 * "report_*" parameters indicate that a given type of events should be 
 * reported, all events of that type will be reported no matter in which 
 * function and in which thread they occur. */
static int restrict_to_func = 0;

/* The number of events reported in the current session so far. */
static unsigned int ecount = 0;

/* The start address of the target function. */
static unsigned long target_start = 0;

#define KEDR_ALL_THREADS ((unsigned long)(-1))

/* The ID of the thread to report the events for. If it is KEDR_ALL_THREADS,
 * no restriction on thread ID is imposed. */
static unsigned long target_tid = KEDR_ALL_THREADS;
/* ====================================================================== */

/* The structures containing the data to be passed to the workqueue. 
 * See core_api.h for the description of the fields (except 'work'). */

/* Data for function entry/exit events. */
struct kr_work_on_func
{
	struct work_struct work;
	unsigned long tid;
	void *func;
};

/* Data for call pre/post events. */
struct kr_work_on_call
{
	struct work_struct work;
	unsigned long tid;
	void *pc;
	void *func;
};
/* ====================================================================== */

/* This function will be called for each symbol known to the system.
 * We need to find only the particular function in the target module.
 *
 * 'data' should be the pointer to the struct module for the target.
 *
 * If this function returns 0, kallsyms_on_each_symbol() will continue
 * walking the symbols. If non-zero, it will stop. */
static int
symbol_walk_callback(void *data, const char *name, struct module *mod, 
	unsigned long addr)
{
	/* Skip the symbol if it does not belong to the target module. */
	if (mod != (struct module *)data) 
		return 0;
	
	if (strcmp(name, target_function) == 0) {
		target_start = addr;
		return 1; /* no need to search further */
	}
	return 0;
}
/* ====================================================================== */

/* Clears the output. 
 * 'work' is not expected to be contained in any other structure. */
static void 
work_func_clear(struct work_struct *work)
{
	debug_util_clear();
	kfree(work);
}

/* Reports function entry event.
 * 'work' should be &kr_work_on_func::work. */
static void 
work_func_entry(struct work_struct *work)
{
	static const char *fmt = "TID=0x%lx FENTRY name=\"%pf\"\n";
	int ret;
	struct kr_work_on_func *wof = container_of(work, 
		struct kr_work_on_func, work);
	
	ret = debug_util_print(fmt, wof->tid, wof->func);
	if (ret < 0)
		pr_warning(KEDR_MSG_PREFIX 
		"work_func_entry(): output failed, error code: %d.\n", 
			ret);
	kfree(wof);
}

/* Reports function exit event.
 * 'work' should be &kr_work_on_func::work. */
static void 
work_func_exit(struct work_struct *work)
{
	static const char *fmt = "TID=0x%lx FEXIT name=\"%pf\"\n";
	int ret;
	struct kr_work_on_func *wof = container_of(work, 
		struct kr_work_on_func, work);
	
	ret = debug_util_print(fmt, wof->tid, wof->func);
	if (ret < 0)
		pr_warning(KEDR_MSG_PREFIX 
		"work_func_exit(): output failed, error code: %d.\n", 
			ret);
	kfree(wof);
}

/* Reports "call pre" event.
 * 'work' should be &kr_work_on_call::work. */
static void 
work_func_call_pre(struct work_struct *work)
{
	static const char *fmt = "TID=0x%lx CALL_PRE pc=%pS name=\"%pf\"\n";
	int ret;	
	struct kr_work_on_call *woc = container_of(work, 
		struct kr_work_on_call, work);
	
	ret = debug_util_print(fmt, woc->tid, woc->pc, woc->func);
	if (ret < 0)
		pr_warning(KEDR_MSG_PREFIX 
		"work_func_call_pre(): output failed, error code: %d.\n", 
			ret);
	kfree(woc);
}

/* Reports "call post" event.
 * 'work' should be &kr_work_on_call::work. */
static void 
work_func_call_post(struct work_struct *work)
{
	static const char *fmt = 
		"TID=0x%lx CALL_POST pc=%pS name=\"%pf\"\n";
	int ret;
	struct kr_work_on_call *woc = container_of(work, 
		struct kr_work_on_call, work);
	
	ret = debug_util_print(fmt, woc->tid, woc->pc, woc->func);
	if (ret < 0)
		pr_warning(KEDR_MSG_PREFIX 
		"work_func_call_post(): output failed, error code: %d.\n", 
			ret);
	kfree(woc);
}
/* ====================================================================== */

/* If the function is called not from on_load/on_unload handlers, 'wq_lock' 
 * must be held. */
static void
reset_counters(void)
{
	target_start = 0;
	target_tid = KEDR_ALL_THREADS;
	ecount = 0;
}

/* If the function is called not from on_load/on_unload handlers, 'wq_lock' 
 * must be held. 
 * 
 * Returns non-zero if it is allowed to report the event with a given TID
 * provided "report_*" parameters also allow that. 0 if the event should not 
 * be reported. */
static int
report_event_allowed(unsigned long tid)
{
	if (ecount >= max_events)
		return 0;
	
	if (!restrict_to_func)
		return 1;
	
	return (within_target_func && (tid == target_tid));
}
 
/* ====================================================================== */

static void 
on_load(struct kedr_event_handlers *eh, struct module *target_module)
{
	int ret;
	
	reset_counters();
	debug_util_clear();	
	
	if (!restrict_to_func)
		return;
	
	ret = kallsyms_on_each_symbol(symbol_walk_callback, target_module);
	if (ret < 0) {
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to search for the function \"%s\".\n", 
			target_function);
	}
	else if (ret == 0) {
		pr_info(KEDR_MSG_PREFIX 
			"The function \"%s\" was not found in \"%s\".\n", 
			target_function, module_name(target_module));
	}
	else { /* Must have found the target function. */
		BUG_ON(target_start == 0);
	}
}

static void 
on_unload(struct kedr_event_handlers *eh, struct module *target_module)
{
	flush_workqueue(wq);
}

static void 
on_function_entry(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long func)
{
	unsigned long irq_flags;
	struct work_struct *work = NULL;
	struct kr_work_on_func *wof = NULL;

	spin_lock_irqsave(&wq_lock, irq_flags);
	if (func == target_start) {
		/* Another entry to the target function detected but the
		 * previous invocation of that function has not exited yet.
		 * May be a recursive call or a call from another thread. 
		 * The report may contain less data than expected. */
		WARN_ON_ONCE(within_target_func != 0);
		within_target_func = 1;
		target_tid = tid;
		ecount = 0;
		
		/* Add a command to the wq to clear the output */
		work = kzalloc(sizeof(*work), GFP_ATOMIC);
		if (work == NULL) {
			pr_warning(KEDR_MSG_PREFIX 
			"on_function_entry(): out of memory.\n");
			goto out;
		}
		INIT_WORK(work, work_func_clear);
		queue_work(wq, work);
	}
	if (!report_calls || !report_event_allowed(tid))
		goto out;
	++ecount;
	
	wof = kzalloc(sizeof(*wof), GFP_ATOMIC);
	if (wof == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"on_function_entry(): no memory for \"wof\".\n");
			goto out;
	}
	wof->tid = tid;
	wof->func = (void *)func;
	INIT_WORK(&wof->work, work_func_entry);
	queue_work(wq, &wof->work);

out:	
	spin_unlock_irqrestore(&wq_lock, irq_flags);
}

static void 
on_function_exit(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long func)
{
	unsigned long irq_flags;
	struct kr_work_on_func *wof = NULL;
	
	spin_lock_irqsave(&wq_lock, irq_flags);
	if (!report_calls || !report_event_allowed(tid))
		goto out;
	++ecount;
	
	wof = kzalloc(sizeof(*wof), GFP_ATOMIC);
	if (wof == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"on_function_exit(): out of memory.\n");
			goto out;
	}
	wof->tid = tid;
	wof->func = (void *)func;
	INIT_WORK(&wof->work, work_func_exit);
	queue_work(wq, &wof->work);

out:	
	if (func == target_start && tid == target_tid) {
		/* Warn if it is an exit from the target function but no 
		 * entry event has been received for it. */
		WARN_ON_ONCE(within_target_func == 0);
		within_target_func = 0;
		target_tid = KEDR_ALL_THREADS;
	}
	spin_unlock_irqrestore(&wq_lock, irq_flags);
}

static void 
on_call_pre(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long func)
{
	unsigned long irq_flags;
	struct kr_work_on_call *woc = NULL;
	
	spin_lock_irqsave(&wq_lock, irq_flags);
	if (!report_calls || !report_event_allowed(tid))
		goto out;
	++ecount;
	
	woc = kzalloc(sizeof(*woc), GFP_ATOMIC);
	if (woc == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"on_call_pre(): out of memory.\n");
			goto out;
	}
	woc->tid = tid;
	woc->pc = (void *)pc;
	woc->func = (void *)func;
	INIT_WORK(&woc->work, work_func_call_pre);
	queue_work(wq, &woc->work);
out:
	spin_unlock_irqrestore(&wq_lock, irq_flags);
}

static void 
on_call_post(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long func)
{
	unsigned long irq_flags;
	struct kr_work_on_call *woc = NULL;
	
	spin_lock_irqsave(&wq_lock, irq_flags);
	if (!report_calls || !report_event_allowed(tid))
		goto out;
	++ecount;

	woc = kzalloc(sizeof(*woc), GFP_ATOMIC);
	if (woc == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"on_call_post(): out of memory.\n");
			goto out;
	}
	woc->tid = tid;
	woc->pc = (void *)pc;
	woc->func = (void *)func;
	INIT_WORK(&woc->work, work_func_call_post);
	queue_work(wq, &woc->work);
out:
	spin_unlock_irqrestore(&wq_lock, irq_flags);
}

struct kedr_event_handlers eh = {
	.owner = THIS_MODULE,
	.on_target_loaded = on_load,
	.on_target_about_to_unload = on_unload,
	.on_function_entry = on_function_entry,
	.on_function_exit = on_function_exit,
	.on_call_pre = on_call_pre,
	.on_call_post = on_call_post,
	/* [NB] Add more handlers here if necessary. */
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_unregister_event_handlers(&eh);
	
	destroy_workqueue(wq);
	debug_util_fini();
	debugfs_remove(debugfs_dir_dentry);
	return;
}

static int __init
test_init_module(void)
{
	int ret = 0;
	
	restrict_to_func = (target_function[0] != 0);
	
	/* [NB] Add checking of other report_* parameters here as needed. */
	if (report_calls == 0) {
		pr_warning(KEDR_MSG_PREFIX 
	"At least one of \"report_*\" parameters should be non-zero.\n");
		return -EINVAL;
	}
	
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

	ret = debug_util_init(debugfs_dir_dentry);
	if (ret != 0)
		goto out_rmdir;
	
	wq = create_singlethread_workqueue(wq_name);
	if (wq == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to create workqueue \"%s\"\n",
			wq_name);
		ret = -ENOMEM;
		goto out_clean_debug;
	}
	
	/* [NB] Register event handlers only after everything else has 
	 * been initialized. */
	ret = kedr_register_event_handlers(&eh);
	if (ret != 0)
		goto out_clean_all;
	
	return 0;

out_clean_all:
	destroy_workqueue(wq);
out_clean_debug:	
	debug_util_fini();
out_rmdir:
	debugfs_remove(debugfs_dir_dentry);
out:
	return ret;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
