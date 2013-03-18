/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Author: 
 *      Eugene A. Shatokhin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#include "config.h"
/* ====================================================================== */
<$if concat(header)$>
<$header: join(\n)$>
/* ====================================================================== */
<$endif$>
#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

extern unsigned long kedr_system_wq_id;
/* ====================================================================== */

/* To save space and simplify the code, the most significant bit of the 
 * address of a workqueue is used to store a flag "normal/ordered". The
 * flag is 1 for a normal (unordered) workqueue, 0 for an ordered one. 
 * Using that bit is possible because the higher bit of a kernel address is 
 * always 1 in the VM split configurations we support on x86. */
#ifdef CONFIG_X86_64
# define KEDR_WQ_NORMAL_MASK (1UL << 63)
#else /* CONFIG_X86_32 */
# define KEDR_WQ_NORMAL_MASK (1UL << 31)
#endif

/* The following happens-before relations are expressed here.
 *
 * 1. Start of a function that queues/schedules a work item happens-before
 * the start of the work function in that item.
 * ID: &work. 
 *
 * 2. End of a work function for a work item happens-before the end of 
 * flush_work(), cancel_work_sync() and their "_delayed" counterparts called
 * for that item.
 * ID: &work + 1. 
 * sizeof(struct work_struct) is not less that sizeof(void *) anyway. 
 *
 * 3. End of a work function called for a work item happens-before the 
 * start of this or another work function for the same work item.
 * ID: &work + 2.
 * This is a simplification. This rule is based on the assumption that a
 * work item cannot be processed concurrently. We assume that a given work
 * item can be used in one workqueue only and that the item is not required 
 * to be reentrant. The former seems feasible in all kernel versions we 
 * support, the latter is the case since 3.7 only.
 *
 * 4. End of a work function for a work item happens-before the end of 
 * flush, drain, destroy and similar functions for the whole workqueue.
 * ID: &workqueue if the wq has been created by the target modules,  
 * kedr_system_wq_id for a system-wide wq.
 * Here we assume again that a work item can be used for one wq only.
 *
 * 5. (Ordered workqueues only.) End of a work function for a work item
 * happens-before the start of any work function for any work item executed
 * in the same wq after this item. In other words, there is no concurrent 
 * execution in this workqueue.
 * ID: same as in #4. */

/* Information about a wq created by one of the target modules.
 * 'id' is the address of the workqueue except that its most significant
 * bit is set to 0 for an ordered wq and left 1 otherwise. */
struct kedr_fh_workqueue_info
{
	struct list_head list;
	unsigned long id;
};

/* The list of kedr_fh_workqueue_info object. */
static LIST_HEAD(wq_info);

/* All accesses to 'wq_info' list must be protected by this lock. The only
 * exception is the cleanup function for this group. The lock is not needed 
 * there because the function runs during unloading of the plugin. No 
 * other thread could try to access the list then. */
static DEFINE_SPINLOCK(wq_info_lock);
/* ====================================================================== */

/* The cleanup function will be called from the exit function of
 * the FH plugin. */
void
kedr_fh_drd_workqueue_cleanup(void)
{
	struct kedr_fh_workqueue_info *info;
	struct kedr_fh_workqueue_info *tmp;

	list_for_each_entry_safe(info, tmp, &wq_info, list) {
		list_del(&info->list);
		kfree(info);
	}
}

/* Must be called with 'wq_info_lock' locked. */
static struct kedr_fh_workqueue_info *
workqueue_find_info_impl(struct workqueue_struct * wq)
{
	unsigned long id = (unsigned long)wq;
	struct kedr_fh_workqueue_info *info;

	list_for_each_entry(info, &wq_info, list) {
		if ((info->id | KEDR_WQ_NORMAL_MASK) == id)
			return info;
	}
	return NULL;
}

/* Call this function for a newly created wq. */
static void
workqueue_add_info(struct workqueue_struct *wq, int ordered)
{
	struct kedr_fh_workqueue_info *info;
	unsigned long flags;
	
	spin_lock_irqsave(&wq_info_lock, flags);

	info = workqueue_find_info_impl(wq);
	if (info != NULL) {
		pr_warning(KEDR_MSG_PREFIX
	"Attempt to add info about an already known workqueue (%p).\n",
			wq);
		pr_warning(KEDR_MSG_PREFIX
			"The old info will be deleted.\n");
		list_del(&info->list);
		kfree(info);
	}

	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	if (info != NULL) {
		info->id = (unsigned long)wq;
		WARN_ON_ONCE((info->id & KEDR_WQ_NORMAL_MASK) == 0);
		if (ordered)
			info->id &= ~KEDR_WQ_NORMAL_MASK;

		list_add(&info->list, &wq_info);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX
		"workqueue_add_info(): not enough memory.\n");
	}
	
	spin_unlock_irqrestore(&wq_info_lock, flags);
}

/* Call this function right before the wq is destroyed. */
static void
workqueue_remove_info(struct workqueue_struct *wq)
{
	struct kedr_fh_workqueue_info *info;
	unsigned long flags;

	spin_lock_irqsave(&wq_info_lock, flags);

	info = workqueue_find_info_impl(wq);
	if (info != NULL) {
		list_del(&info->list);
		kfree(info);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX "Unknown workqueue: %p.\n", wq);
	}

	spin_unlock_irqrestore(&wq_info_lock, flags);
}

/* The returned value is the ID for the happens-before arcs for the wq
 * except for the most significant bit. Set it to 1 to get the real ID.
 * The bit indicates if the wq is normal (1) or ordered (0).
 * 
 * System wqs are considered normal.
 *
 * For unknown wqs, the ID of the system wq is returned. */
static unsigned long
workqueue_get_id(struct workqueue_struct *wq)
{
	struct kedr_fh_workqueue_info *info;
	unsigned long flags;
	unsigned long id;

	spin_lock_irqsave(&wq_info_lock, flags);

	info = workqueue_find_info_impl(wq);
	if (info != NULL)
		id = info->id;
	else
		id = kedr_system_wq_id;
	
	spin_unlock_irqrestore(&wq_info_lock, flags);

	return id;
}

static inline int
is_wq_ordered(unsigned long info)
{
	return !(info & KEDR_WQ_NORMAL_MASK);
}

static inline unsigned long
id_from_wq_info(unsigned long info)
{
	return (info | KEDR_WQ_NORMAL_MASK);
}
/* ====================================================================== */

static void
work_func_pre(struct kedr_local_storage *ls)
{
	unsigned long tid = kedr_get_thread_id();
	unsigned long pc = ls->fi->addr;
	void *data;
	unsigned long work = KEDR_LS_ARG1(ls);
	unsigned long id;

	/* The callback handlers are already executed within RCU read-side
	 * section, so it is not needed to use rcu_read_lock/unlock here.
	 * rcu_dereference() IS needed, however. */
	data = rcu_dereference(ls->fi->data);
	if (data == NULL)
		pr_warning(KEDR_MSG_PREFIX
			"work_func_pre(): 'data' is NULL.\n");

	id = id_from_wq_info((unsigned long)data);
		
	/* Relation #1 */
	kedr_happens_after(tid, pc, work);

	/* Relation #3. */
	kedr_happens_after(tid, pc, work + 2);

	/* Relation #5. */
	if (is_wq_ordered((unsigned long)data))
		kedr_happens_after(tid, pc, id);
}

static void
work_func_post(struct kedr_local_storage *ls)
{
	unsigned long tid = kedr_get_thread_id();

	/* The callback handlers cannot use 'ls->info' as call info to
	 * determine the PC: ls->info contains the call information only for
	 * the calls made from the target modules but the callback may have
	 * been called from elsewhere. So, the address of the callback is
	 * used here as the location in the code where the events are
	 * reported. */
	unsigned long pc = ls->fi->addr;

	void *data;
	unsigned long work = KEDR_LS_ARG1(ls);
	unsigned long id;

	data = rcu_dereference(ls->fi->data);
	if (data == NULL)
		pr_warning(KEDR_MSG_PREFIX
			"work_func_post(): 'data' is NULL.\n");

	id = id_from_wq_info((unsigned long)data);
		
	/* Relation #2. */
	kedr_happens_before(tid, pc, work + 1);

	/* Relation #3. */
	kedr_happens_before(tid, pc, work + 2);

	/* Relations #4 and #5. */
	kedr_happens_before(tid, pc, id);
}

/* Sets pre- and post- handlers for a work function and wq info as the data
 * that should be available for these functions. */
static void
set_work_func_handlers(struct work_struct *work, unsigned long wq_id)
{
	struct kedr_func_info *fi;
	unsigned long flags;

	if (work == NULL || work->func == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"set_work_func_handlers(): invalid work structure.\n");
		return;
	}

	fi = kedr_find_func_info((unsigned long)work->func);
	if (fi == NULL) /* A non-instrumentable or unknown function. */
		return;

	/* If a handler is already set, it must remain unchanged.
	 * The wq info will be stored in 'data' not matter what is already
	 * there. */
	spin_lock_irqsave(&fi->handler_lock, flags);
	if (fi->pre_handler == NULL)
		rcu_assign_pointer(fi->pre_handler, work_func_pre);
	if (fi->post_handler == NULL)
		rcu_assign_pointer(fi->post_handler, work_func_post);

	fi->data = (void *)wq_id;
	spin_unlock_irqrestore(&fi->handler_lock, flags);
}

static void
on_wq_flush_drain(struct kedr_local_storage *ls, unsigned long wq)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	if (wq == 0)
		return;
	
	/* Relation #4 */
	kedr_happens_after(tid, info->pc, wq);
}

static void
on_queue_work(struct kedr_local_storage *ls, struct work_struct *work,
	      unsigned long wq_id)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	/* Relation #1 */
	kedr_happens_before(tid, info->pc, (unsigned long)work);

	set_work_func_handlers(work, wq_id);
}

static void
on_flush_or_cancel_work(struct kedr_local_storage *ls,
			struct work_struct *work)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	/* Relation #2 */
	kedr_happens_after(tid, info->pc, (unsigned long)work + 1);
}
/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */
<$endif$>
