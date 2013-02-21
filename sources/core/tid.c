/* tid.c - the means to obtain thread ID as well as to maintain the global 
 * sampling-related information (thread indexes). */

/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin
 *      Andrey V. Tsyvarev
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>		/* smp_processor_id() */
#include <linux/sched.h>	/* current */
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/percpu.h>

#include <kedr/kedr_mem/core_api.h>

#include "config.h"
#include "core_impl.h"

#include "tid.h"
/* ====================================================================== */

#ifndef __percpu
/* This "specifier" was introduced in the kernel 2.6.33. Dynamically
 * allocated per-CPU variables were declared without it in 2.6.32. To make
 * the code uniform, define __percpu here. */
#define __percpu
#endif
/* ====================================================================== */

/* [NB] We only need to process the handlers for hardware interrupts
 * separately from the code running in process and softirq contexts. The
 * code running in these contexts runs in the appropriate threads, 'current'
 * is valid there.
 * In addition, it is undesirable to use in_interrupt() here as it may
 * return non-zero in the process context too, e.g., in the critical
 * sections with a spinlock locked with spin_lock_bh(). */
unsigned long
kedr_get_thread_id(void)
{
	return (kedr_in_interrupt() ?
		(unsigned long)smp_processor_id() :
		(unsigned long)current);
}
EXPORT_SYMBOL(kedr_get_thread_id);
/* ====================================================================== */

/* 0 if no hardirq handlers provided by the target have been executed on
 * this CPU so far, non-zero otherwise. Used to generate "thread start"
 * events for "IRQ pseudo threads". */
int __percpu *kedr_known_irq_thread;

/* 0 if the swapper thread for the current CPU have not entered the targets
 * so far, non-zero otherwise.
 * The task structs for swapper threads (threads of the process with
 * PID == 0) are not traversed by do_each_thread/while_each_thread. As they
 * are always "running", it makes no sense to put information about them
 * into the thread table. It is enough to report the first appearance of
 * such threads. */
int __percpu *kedr_known_swapper_thread;
/* ====================================================================== */

/* The lock to protect the updates of the thread table and other global
 * data. */
static DEFINE_SPINLOCK(upd_lock);

/* The hash table {thread ID, thread info} is used to keep record of the
 * threads that executed the code of the targets and to generate
 * "thread start" and "thread end" events appropriately.
 * Note that the pseudo threads for hardirqs are not represented here, they
 * are handled separately.
 * 
 * KEDR_THREAD_TABLE_SIZE - number of buckets in the hash table. */
#define KEDR_THREAD_TABLE_HASH_BITS   10
#define KEDR_THREAD_TABLE_SIZE   (1 << KEDR_THREAD_TABLE_HASH_BITS)

struct kedr_thread_info
{
	/* The next element in the bucket, NULL for the last element.
	 * [NB] Access it in an RCU-aware way. */
	struct kedr_thread_info *next;

	/* ID of the thread.
	 * [NB] When accessing kedr_thread_info instance, do not assume
	 * 'tid' is an address of a valid task_struct, the latter might have
	 * been deleted already. */
	unsigned long tid;

	/* Start time of the thread, see task_struct::real_start_time.
	 * It is used to detect the new threads with the same addresses
	 * of task_struct instances as some already ended threads. */
	struct timespec real_start_time;
};

/* The hash table {thread ID, thread info}.
 * Reading a bucket from this table requries rcu_read_lock/unlock.
 * Reading *(thread_table[i]) requires rcu_dereference().
 *
 * If it is the first appearance of the given thread in the target module,
 * the corresponding bucket should be copied as a whole, the copy should be 
 * updated and then set instead of the old one, all this under 'upd_lock'. 
 * The stale buckets should be deleted by RCU callbacks.
 *
 * Same for other kinds of updates of the table.
 *
 * Note that an entry for a thread can only be added in the table by that
 * very thread. */
static struct kedr_thread_info **thread_table = NULL;

/* A copy of the thread table used by the garbage collector. */
static struct kedr_thread_info **shadow_table = NULL;

/* This timer is used to launch the "garbage collector" (GC) for the thread
 * table to remove the items for the ended threads from there. */
static struct timer_list gc_timer;

/* 1 - the timer function will re-register itself, 0 - it will not. */
atomic_t gc_timer_repeat = ATOMIC_INIT(0);
/* ====================================================================== */

/* The GC uses the higher bit of 'tid' field for "live/dead" flag. Except
 * for some exotic VM split configurations, that bit of a task_struct 
 * address is always 1 on x86, so it can be used to store that flag. */
#ifdef CONFIG_X86_64
# define KEDR_LIVE_THREAD_MASK (1UL << 63)
#else /* CONFIG_X86_32 */
# define KEDR_LIVE_THREAD_MASK (1UL << 31)
#endif

static void 
mark_thread_live(struct kedr_thread_info *item)
{
	item->tid |= KEDR_LIVE_THREAD_MASK;
}

static void 
mark_thread_dead(struct kedr_thread_info *item)
{
	/* It should also be checked by CMake scripts that the virtual
	 * memory addresses are split between the user space and the kernel
	 * space appropriately. Added a warning here too, just in case. */
	WARN_ON_ONCE((item->tid & KEDR_LIVE_THREAD_MASK) == 0);
	
	item->tid &= ~KEDR_LIVE_THREAD_MASK;
}

static int
is_thread_live(struct kedr_thread_info *item)
{
	return (item->tid & KEDR_LIVE_THREAD_MASK) != 0;
}

static int
is_same_thread(struct task_struct *task, struct kedr_thread_info *item)
{
	unsigned long tid = (unsigned long)task;
	if (tid != (item->tid | KEDR_LIVE_THREAD_MASK))
		return 0;
	
	if (task->real_start_time.tv_sec != item->real_start_time.tv_sec ||
	    task->real_start_time.tv_nsec != item->real_start_time.tv_nsec)
	        return 0;

	return 1;
}
/* ====================================================================== */

/* Deletes the bucket starting with the given item. */
static void
delete_bucket(struct kedr_thread_info *item)
{
	struct kedr_thread_info *next;
	while (item != NULL) {
		next = item->next;
		kfree(item);
		item = next;
	}
}

/* Make sure that noone can access the thread table when this function runs:
 * use rcu_barrier() if needed before it, etc.*/
static void
clear_thread_table(void)
{
	int i;
	BUG_ON(thread_table == NULL);

	for (i = 0; i < KEDR_THREAD_TABLE_SIZE; ++i) {
		delete_bucket(thread_table[i]);
		thread_table[i] = NULL;
	}
}

static void
thread_handle_changes_irq(void)
{
	int *ptr = NULL;

	unsigned long id = get_cpu();
	ptr = per_cpu_ptr(kedr_known_irq_thread, id);
	if (*ptr == 0) {
		char comm[TASK_COMM_LEN];

		memset(&comm[0], 0, sizeof(comm));
		snprintf(&comm[0], sizeof(comm), "irq%lu", id);
		kedr_eh_on_thread_start(id, &comm[0]);

		*ptr = 1;
	}
	put_cpu();
}

static void
thread_handle_changes_swapper(struct task_struct *task)
{
	int *ptr = NULL;

	unsigned long id = get_cpu();
	ptr = per_cpu_ptr(kedr_known_swapper_thread, id);
	if (*ptr == 0) {
		kedr_eh_on_thread_start((unsigned long)task, task->comm);
		*ptr = 1;
	}
	put_cpu();
}

/* Creates an updated copy of the given bucket. Returns the pointer to the
 * created bucket if successful (can be NULL if the resulting bucket
 * contains no elements), ERR_PTR(-errno) on error.
 *
 * [NB] 'old_bucket' is NULL if this is the first time something should be
 * placed in this bucket.
 *
 * If 'task' is not NULL, the information about the thread with the
 * specified task_struct will be placed at the beginning of the new bucket.
 * In addition, the function will not include the items from the old bucket
 * that have the same thread ID as the new thread into the new bucket in
 * this case.
 *
 * Should be called with 'upd_lock' locked. */
static struct kedr_thread_info *
create_new_bucket(const struct kedr_thread_info *old_bucket,
		  struct task_struct *task)
{
	struct kedr_thread_info base;
	struct kedr_thread_info *item;
	const struct kedr_thread_info *old_item;

	memset(&base, 0, sizeof(base));
	
	if (task != NULL) {
		item = kzalloc(sizeof(*item), GFP_ATOMIC);
		if (item == NULL)
			return ERR_PTR(-ENOMEM);

		item->tid = (unsigned long)task;
		item->real_start_time.tv_sec = 
			task->real_start_time.tv_sec;
		item->real_start_time.tv_nsec = 
			task->real_start_time.tv_nsec;

		base.next = item;
	}
	else {
		item = &base;
	}

	old_item = old_bucket;
	for (; old_item != NULL; old_item = old_item->next) {
		struct kedr_thread_info *new_item;

		/* It is assumed that the entry for 'task' was not added to
		 * the table before this function was called. That is, if
		 * we find an entry with the corresponding TID, it is for a
		 * thread that has already finished. Report that it has. */
		if (old_item->tid == (unsigned long)task) {
			kedr_eh_on_thread_end((unsigned long)task);
			continue;
		}
		
		new_item = kzalloc(sizeof(*new_item), GFP_ATOMIC);
		if (new_item == NULL) {
			delete_bucket(base.next);
			return ERR_PTR(-ENOMEM);
		}

		new_item->tid = old_item->tid;
		new_item->real_start_time.tv_sec = 
			old_item->real_start_time.tv_sec;
		new_item->real_start_time.tv_nsec = 
			old_item->real_start_time.tv_nsec;

		item->next = new_item;
		item = new_item;
	}
	return base.next;
}

struct kedr_thread_bucket_rcu
{
	struct rcu_head rcu;
	struct kedr_thread_info *bucket;
};

static void
reclaim_bucket(struct rcu_head *rp)
{
	struct kedr_thread_bucket_rcu *p =
		container_of(rp, struct kedr_thread_bucket_rcu, rcu);
	delete_bucket(p->bucket);
	kfree(p);
}

/* Add an item about the current thread ('task') to the table and report
 * that the thread has started (well, entered the target modules, to be
 * exact).
 * Note that no other running thread could have added an entry for the same
 * TID after kedr_thread_handle_changes() detected that this is a new one.
 * No other running thread can have the same address of the task struct
 * as the thread in question, so there is no race window here. */
static int
add_thread_info(struct task_struct *task)
{
	unsigned long irq_flags;
	unsigned long i;
	struct kedr_thread_info *old_bucket;
	struct kedr_thread_info *new_bucket;
	struct kedr_thread_bucket_rcu *tbr = NULL;

	i = hash_long((unsigned long)task, KEDR_THREAD_TABLE_HASH_BITS);
	spin_lock_irqsave(&upd_lock, irq_flags);

	old_bucket = thread_table[i];
	if (old_bucket != NULL) {
		/* Create everything necessary to delete the old bucket
		 * later. */
		tbr = kzalloc(sizeof(*tbr), GFP_ATOMIC);
		if (tbr == NULL) {
			spin_unlock_irqrestore(&upd_lock, irq_flags);
			return -ENOMEM;
		}
		tbr->bucket = old_bucket;
	}

	/* [NB] If there was an item with the same TID as 'task' but for a
	 * different thread and the "garbage collector" has deleted that
	 * item before we have locked 'upd_lock', this is also acceptable.*/
	new_bucket = create_new_bucket(old_bucket, task);
	if (IS_ERR(new_bucket)) {
		spin_unlock_irqrestore(&upd_lock, irq_flags);
		kfree(tbr);
		return PTR_ERR(new_bucket);
	}

	rcu_assign_pointer(thread_table[i], new_bucket);
	kedr_eh_on_thread_start((unsigned long)task, &task->comm[0]);
	
	/* Schedule deletion of the old bucket */
	if (tbr != NULL)
		call_rcu(&tbr->rcu, reclaim_bucket);

	spin_unlock_irqrestore(&upd_lock, irq_flags);
	return 0;
}
/* ====================================================================== */

static void
clear_shadow_table(void)
{
	int i;
	for (i = 0; i < KEDR_THREAD_TABLE_SIZE; ++i) {
		delete_bucket(shadow_table[i]);
		shadow_table[i] = NULL;
	}
}

/* Copies the thread table to the shadow table and marks all the elements
 * of the latter as "dead". 
 * [NB] If copying fails, the table may be left partially filled. The caller
 * should use clear_shadow_table() to clear it then. */
static int 
copy_thread_table(void)
{
	int i;
	struct kedr_thread_info *item;
	
	memset(shadow_table, 0, 
		KEDR_THREAD_TABLE_SIZE * sizeof(shadow_table[0]));
	
	for (i = 0; i < KEDR_THREAD_TABLE_SIZE; ++i) {
		if (thread_table[i] == NULL)
			continue;
		
		shadow_table[i] = create_new_bucket(thread_table[i], NULL);
		if (IS_ERR(shadow_table[i])) {
			int err = PTR_ERR(shadow_table[i]);
			shadow_table[i] = NULL;
			pr_warning(KEDR_MSG_PREFIX 
		"Failed to copy the thread table, error code: %d.\n",
				err);
			return err;
		}
		
		/* Mark all items in the newly created bucket as "dead". */
		item = shadow_table[i];
		for (; item != NULL; item = item->next)
			mark_thread_dead(item);
	}
	return 0;
}

/* Remove the "dead" items from the bucket with the given index in the 
 * shadow table, generate "thread end" events accordingly.
 * The function returns non-zero if it has removed at least one item,
 * 0 otherwise.
 * Must be called with 'upd_lock' locked. */
static int
remove_dead_items(unsigned long index)
{
	int changed = 0;
	struct kedr_thread_info **pnext = &shadow_table[index];
	struct kedr_thread_info *item = shadow_table[index];
	
	if (item == NULL)
		return 0;
	
	for (; item != NULL; item = *pnext) {
		if (is_thread_live(item)) {
			pnext = &item->next;
		}
		else {
			*pnext = item->next;
			kedr_eh_on_thread_end(
				item->tid | KEDR_LIVE_THREAD_MASK);
			kfree(item);
			changed = 1;
		}
	}
	return changed;
}

/* This function is periodically launched to find the items in the thread 
 * table that correspond to already ended ("dead") threads and to remove
 * such items. */
static void
gc_timer_fn(unsigned long arg)
{
	unsigned long irq_flags;
	unsigned long i;
	
	struct kedr_thread_info *item;
	struct task_struct *g = NULL;
	struct task_struct *t = NULL;
	
	spin_lock_irqsave(&upd_lock, irq_flags);
	if (copy_thread_table() != 0)
		goto out;

	rcu_read_lock();
	do_each_thread(g, t) {
		/* Check if the there is an item for the thread 't' in the
		 * shadow table. If it is, mark it as "live". */
		i = hash_long((unsigned long)t, 
			KEDR_THREAD_TABLE_HASH_BITS);
		item = shadow_table[i];
		for (; item != NULL; item = item->next) {
			if (is_same_thread(t, item)) {
				mark_thread_live(item);
				break;
			}
		}
	} while_each_thread(g, t);
	rcu_read_unlock();
	
	/* Now, find the "dead" items, report "thread end", delete them,
	 * update the thread table and schedule deletion of its original
	 * buckets. */
	for (i = 0; i < KEDR_THREAD_TABLE_SIZE; ++i) {
		struct kedr_thread_bucket_rcu *tbr = NULL;
		
		if (shadow_table[i] == NULL)
			continue;
		
		if (remove_dead_items(i) == 0) {
			/* There was nothing to remove. */
			delete_bucket(shadow_table[i]);
			shadow_table[i] = NULL;
			continue;
		}
		
		/* Create everything necessary to delete the old bucket
		 * later. */
		tbr = kzalloc(sizeof(*tbr), GFP_ATOMIC);
		if (tbr == NULL) {
			pr_warning(KEDR_MSG_PREFIX 
	"Not enough memory to prepare a structure for RCU callback.\n");
			goto out;
		}
		tbr->bucket = thread_table[i];
		
		rcu_assign_pointer(thread_table[i], shadow_table[i]);
		shadow_table[i] = NULL;
		
		call_rcu(&tbr->rcu, reclaim_bucket);
	}
out:	
	clear_shadow_table();
	spin_unlock_irqrestore(&upd_lock, irq_flags);

	if (atomic_dec_and_test(&gc_timer_repeat)) {
		/* Re-registration is allowed. */
		atomic_inc(&gc_timer_repeat);
		
		/* gc_timer.data is the timeout (in jiffies) */
		mod_timer(&gc_timer, jiffies + gc_timer.data);
	}
}
/* ====================================================================== */

void
kedr_thread_handling_start(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		int *p;
		p = per_cpu_ptr(kedr_known_irq_thread, cpu);
		*p = 0;

		p = per_cpu_ptr(kedr_known_swapper_thread, cpu);
		*p = 0;
	}

	/* Allow re-registration of the timer func and start the timer. */
	atomic_set(&gc_timer_repeat, 1);
	mod_timer(&gc_timer, jiffies + gc_timer.data);
}

void
kedr_thread_handling_stop(void)
{
	/* Wait until all our RCU callbacks have completed. */
	rcu_barrier();

	/* Disallow re-registration of the timer func and stop the timer. */
	atomic_dec(&gc_timer_repeat);
	del_timer_sync(&gc_timer);

	clear_thread_table();
}

int
kedr_thread_handling_init(unsigned int gc_msec)
{
	kedr_known_irq_thread = alloc_percpu(int);
	if (kedr_known_irq_thread == NULL)
		return -ENOMEM;

	kedr_known_swapper_thread = alloc_percpu(int);
	if (kedr_known_swapper_thread == NULL)
		goto err_free_known_irq;
	
	thread_table = kzalloc(
		KEDR_THREAD_TABLE_SIZE * sizeof(struct kedr_thread_info *),
		GFP_KERNEL);
	if (thread_table == NULL)
		goto err_free_known_swapper;
	
	shadow_table = kzalloc(
		KEDR_THREAD_TABLE_SIZE * sizeof(struct kedr_thread_info *),
		GFP_KERNEL);
	if (shadow_table == NULL)
		goto err_free_table;

	init_timer(&gc_timer);
	gc_timer.function = gc_timer_fn;
	gc_timer.data = msecs_to_jiffies(gc_msec);
	gc_timer.expires = 0; /* to be set by mod_timer() later */

	return 0;

err_free_table:
	kfree(thread_table);
	
err_free_known_swapper:
	free_percpu(kedr_known_swapper_thread);

err_free_known_irq:
	free_percpu(kedr_known_irq_thread);
	return -ENOMEM;
}

void
kedr_thread_handling_cleanup(void)
{
	/* The buckets of the table should have been deleted by
	 * kedr_thread_handling_stop() already. */
	kfree(thread_table);
	kfree(shadow_table);
	free_percpu(kedr_known_swapper_thread);
	free_percpu(kedr_known_irq_thread);
}
/* ====================================================================== */

int
kedr_thread_handle_changes(void)
{
	unsigned long i;
	struct task_struct *task = NULL;
	struct kedr_thread_info *info;
	struct kedr_thread_info *next;
	
	if (kedr_in_interrupt()) {
		thread_handle_changes_irq();
		return 0;
	}

	/* We are not in a hardirq handler, so we must be in a thread. */
	task = current;

	if (task->pid == 0) {
		/* Handle "swapper" threads separately. */
		thread_handle_changes_swapper(task);
		return 0;
	}
	
	i = hash_long((unsigned long)task, KEDR_THREAD_TABLE_HASH_BITS);

	/* Check if the thread is new. */
	rcu_read_lock();
	info = rcu_dereference(thread_table[i]);
	
	while (info != NULL) {
		next = info->next;
		if (info->tid == (unsigned long)task)
			break;
		info = next;
	}

	if (info == NULL) {
		/* The thread is new. */
		rcu_read_unlock();
		return add_thread_info(task);
	}

	if (info->real_start_time.tv_sec != task->real_start_time.tv_sec ||
	    info->real_start_time.tv_nsec != task->real_start_time.tv_nsec)
	{
		/* The thread is new but its task_struct occupies the 
		 * memory block previously occupied by the task_struct of a 
		 * known thread. Therefore, the latter thread has ended. */
		rcu_read_unlock();
		return add_thread_info(task);
	}
	
	rcu_read_unlock();
	/* Now the bucket may go away if needed. */

	return 0;
}
/* ====================================================================== */

unsigned long
kedr_get_tindex(void)
{
	unsigned long hash;
	
	if (sampling_rate == 0)
		return 0;

	if (kedr_in_interrupt()) {
		return (unsigned long)smp_processor_id() %
			KEDR_SAMPLING_NUM_TIDS_IRQ;
	}

	/* For the real threads, we use the lower bits of the hash computed
	 * for their IDs as the index. */
	hash = hash_long((unsigned long)current,
			 KEDR_THREAD_TABLE_HASH_BITS);
	return hash % KEDR_SAMPLING_NUM_TIDS + KEDR_SAMPLING_NUM_TIDS_IRQ;
}
/* ====================================================================== */
