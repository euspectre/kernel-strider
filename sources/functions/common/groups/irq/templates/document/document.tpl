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

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/fh_drd/common.h>
#include <kedr/object_types.h>

#include "config.h"
/* ====================================================================== */
<$if concat(header)$>
<$header: join(\n)$>
/* ====================================================================== */
<$endif$>
#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

/* The following happens-before ("HB") relations are expressed here.
 *
 * 1. Start of *request*irq() HB start of the IRQ handler.
 * ID: id_start(irq_no, dev_id). 
 *
 * 2. End of the IRQ handler HB end of free_irq().
 * ID: id_end(irq_no, dev_id).
 *
 * 3. Local enable/disable with spinlocks, etc., VS IRQ handlers.
 * ID: as set by kedr_irq*_(start|end).
 *
 * 4. End of each IRQ handler for a given IRQ number HB end of 
 * sync_irq() / disable_irq() for that IRQ number.
 * IDs: {id_end(irq_no, dev_id) for a given irq_no}.
 *
 * 5. Start of enable_irq(irq_no) HB start of each IRQ handler for that 
 * IRQ number.
 * IDs: {id_start(irq_no, dev_id) for a given irq_no}.
 *
 * 6. End of the primary IRQ handler HB start of thread_fn for that IRQ
 * (see request_threaded_irq()).
 * ID: id_handlers(irq_no, dev_id)
 */

/* Information about an IRQ used by the target modules: ids of the 
 * involved happens-before arcs. */
struct kedr_fh_irq_info
{
	struct list_head list;
	unsigned int irq;	/* IRQ number */
	void *dev_id; 		/* dev_id passed to *request*irq() */
	
	/* The kedr_fh_irq_info instance will be created in atomic context
	 * so we cannot use kedr_get_unique_id(). We'll allocate an int and
	 * use a pointer to it:
	 * 	id_start = (ulong)id;
	 *	id_end = (ulong)id + 1;
	 *	id_handlers = (ulong)id + 2. */
	void *id;		
};
static LIST_HEAD(irq_info);

/* All accesses to 'irq_info' list must be protected by this lock. The only
 * exception is the cleanup function for this group. The lock is not needed 
 * there because the function runs during unloading of the plugin. No 
 * other thread could try to access the list then. */
static DEFINE_SPINLOCK(irq_info_lock);
/* ====================================================================== */

/* Must be called with 'irq_info_lock' locked. */
static struct kedr_fh_irq_info *
find_info_impl(unsigned int irq, void *dev_id)
{
	struct kedr_fh_irq_info *info;
	list_for_each_entry(info, &irq_info, list) {
		if (info->irq == irq && info->dev_id == dev_id)
			return info;
	}
	return NULL;
}

/* If IRQ info is not yet there for (irq, dev_id) pair, creates
 * that structure and adds it to the list. */
static void
prepare_irq_info(unsigned int irq, void *dev_id)
{
	struct kedr_fh_irq_info *info;
	unsigned long flags;
	
	spin_lock_irqsave(&irq_info_lock, flags);
	info = find_info_impl(irq, dev_id);
	if (info != NULL)
		goto out;
	
	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	if (info == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"prepare_irq_info(): not enough memory.\n");
		goto out;
	}
	
	info->id = kzalloc(sizeof(int), GFP_ATOMIC);
	if (info->id == NULL) {
		kfree(info);
		pr_warning(KEDR_MSG_PREFIX
		"prepare_irq_info(): not enough memory.\n");
		goto out;
	}
	
	info->irq = irq;
	info->dev_id = dev_id;
	list_add(&info->list, &irq_info);
out:
	spin_unlock_irqrestore(&irq_info_lock, flags);
}

static void
free_irq_info(struct kedr_fh_irq_info *info)
{
	if (info != NULL) {
		kfree(info->id);
		kfree(info);
	}
}

/* get_id_*() must be called with 'irq_info_lock' locked. */
static unsigned long
get_id_start(struct kedr_fh_irq_info *info)
{
	return (unsigned long)info->id;
}

static unsigned long
get_id_end(struct kedr_fh_irq_info *info)
{
	return (unsigned long)info->id + 1;
}

static unsigned long
get_id_handlers(struct kedr_fh_irq_info *info)
{
	return (unsigned long)info->id + 2;
}
/* ====================================================================== */

static void
irq_handler_pre(struct kedr_local_storage *ls)
{
	unsigned long tid = kedr_get_thread_id();
	unsigned long pc = ls->fi->addr;
	unsigned int irq = KEDR_LS_ARG1(ls);
	void *dev_id = (void *)KEDR_LS_ARG2(ls);

	struct kedr_fh_irq_info *info;
	unsigned long flags;
	void *data;
	int is_thread_fn;

	data = rcu_dereference(ls->fi->data);
	is_thread_fn = (data == NULL);
	
	/* Relation #3 */
	kedr_irq_start(tid, pc);
	
	spin_lock_irqsave(&irq_info_lock, flags);
	info = find_info_impl(irq, dev_id);
	if (info == NULL)
		goto out;

	/* Relations #1 and #5 */
	kedr_happens_after(tid, pc, get_id_start(info));

	/* Relation #6  */
	if (is_thread_fn) /* the thread function for the IRQ handler */
		kedr_happens_after(tid, pc, get_id_handlers(info));
		
out:
	spin_unlock_irqrestore(&irq_info_lock, flags);
}

static void
irq_handler_post(struct kedr_local_storage *ls)
{
	unsigned long tid = kedr_get_thread_id();
	unsigned long pc = ls->fi->addr;
	unsigned int irq = KEDR_LS_ARG1(ls);
	void *dev_id = (void *)KEDR_LS_ARG2(ls);

	struct kedr_fh_irq_info *info;
	unsigned long flags;
	void *data;
	int is_thread_fn;

	data = rcu_dereference(ls->fi->data);
	is_thread_fn = (data == NULL);

	spin_lock_irqsave(&irq_info_lock, flags);
	info = find_info_impl(irq, dev_id);
	if (info == NULL)
		goto out;

	/* Relation #6  */
	if (!is_thread_fn) /* the primary IRQ handler */
		kedr_happens_before(tid, pc, get_id_handlers(info));

	/* Relations #2 and #4 */
	kedr_happens_before(tid, pc, get_id_end(info));

out:
	spin_unlock_irqrestore(&irq_info_lock, flags);
		
	/* Relation #3 */
	kedr_irq_end(tid, pc);
}

/* Sets pre- and post- handlers for IRQ handlers.
 * The handlers are the same both for the primary IRQ handling functions and
 * for the thread functions used there. To distinguish between these,
 * 'ls->fi->data' will be non-NULL for a primary IRQ handling function and
 * NULL for a thread function. */
static void
set_handlers(irq_handler_t handler, int is_thread_fn)
{
	struct kedr_func_info *fi;
	unsigned long flags;

	fi = kedr_find_func_info((unsigned long)handler);
	if (fi == NULL) /* A non-instrumentable or unknown function. */
		return;

	/* If a handler is already set, it must remain unchanged. */
	spin_lock_irqsave(&fi->handler_lock, flags);
	if (fi->pre_handler == NULL)
		rcu_assign_pointer(fi->pre_handler, irq_handler_pre);
	if (fi->post_handler == NULL)
		rcu_assign_pointer(fi->post_handler, irq_handler_post);

	fi->data = (is_thread_fn ? NULL : (void *)handler);
	spin_unlock_irqrestore(&fi->handler_lock, flags);
}

/* Call this in the pre- handlers of the functions that request the IRQ.
 * One of the handlers may be NULL if not set. */
static void
on_request_irq(struct kedr_local_storage *ls, unsigned int irq,
	       void *dev_id, irq_handler_t handler, irq_handler_t thread_fn)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	struct kedr_fh_irq_info *ii;
	unsigned long flags;
	
	prepare_irq_info(irq, dev_id);

	if (handler != NULL)
		set_handlers(handler, 0);

	if (thread_fn != NULL)
		set_handlers(thread_fn, 1);

	spin_lock_irqsave(&irq_info_lock, flags);
	ii = find_info_impl(irq, dev_id);
	if (ii == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"on_request_irq(): unknown IRQ: (%u, %p).\n",
			irq, dev_id);
		goto out;
	}

	/* Relation #1 */
	kedr_happens_before(tid, info->pc, get_id_start(ii));
out:
	spin_unlock_irqrestore(&irq_info_lock, flags);

}

static void
on_free_irq(struct kedr_local_storage *ls, unsigned int irq, void *dev_id)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	struct kedr_fh_irq_info *ii;
	unsigned long flags;

	spin_lock_irqsave(&irq_info_lock, flags);
	ii = find_info_impl(irq, dev_id);
	if (ii == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"on_free_irq(): unknown IRQ: (%u, %p).\n",
			irq, dev_id);
		goto out;
	}

	/* Relation #2 */
	kedr_happens_after(tid, info->pc, get_id_end(ii));
out:
	spin_unlock_irqrestore(&irq_info_lock, flags);
}

static void
on_enable_irq(struct kedr_local_storage *ls, unsigned int irq)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	struct kedr_fh_irq_info *ii;
	unsigned long flags;

	spin_lock_irqsave(&irq_info_lock, flags);
	list_for_each_entry(ii, &irq_info, list) {
		if (ii->irq != irq)
			continue;

		/* Relation #5 */
		kedr_happens_before(tid, info->pc, get_id_start(ii));
	}
	spin_unlock_irqrestore(&irq_info_lock, flags);
}

static void
on_disable_or_sync_irq(struct kedr_local_storage *ls, unsigned int irq)
{
	unsigned long tid = ls->tid;
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	struct kedr_fh_irq_info *ii;
	unsigned long flags;

	spin_lock_irqsave(&irq_info_lock, flags);
	list_for_each_entry(ii, &irq_info, list) {
		if (ii->irq != irq)
			continue;

		/* Relation #4 */
		kedr_happens_after(tid, info->pc, get_id_end(ii));
	}
	spin_unlock_irqrestore(&irq_info_lock, flags);
}
/* ====================================================================== */

/* The cleanup function will be called from the exit function of
 * the FH plugin. */
void
kedr_fh_drd_irq_cleanup(void)
{
	struct kedr_fh_irq_info *info;
	struct kedr_fh_irq_info *tmp;

	list_for_each_entry_safe(info, tmp, &irq_info, list) {
		list_del(&info->list);
		free_irq_info(info);
	}
}
/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */
<$endif$>
