/* tid.c - the means to obtain thread ID as well as to maintain the global 
 * sampling-related information (thread indexes). */

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
#include <linux/hardirq.h>	/* in_interrupt() */
#include <linux/smp.h>		/* smp_processor_id() */
#include <linux/sched.h>	/* current */

#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/hash.h>
#include <linux/slab.h>

#include <kedr/kedr_mem/core_api.h>

#include "config.h"
#include "core_impl.h"

#include "tid.h"
/* ====================================================================== */

unsigned long
kedr_get_thread_id(void)
{
	return (in_interrupt() 	? (unsigned long)smp_processor_id() 
				: (unsigned long)current);
}
EXPORT_SYMBOL(kedr_get_thread_id);
/* ====================================================================== */

/* The lock to protect the updates of the thread index table and other 
 * global data. */
static DEFINE_SPINLOCK(upd_lock);

/* Parameters of a hash map {thread ID, tindex}.
 * 
 * KEDR_TINDEX_TABLE_SIZE - number of buckets in the table. 
 * KEDR_TINDEX_TABLE_BUCKET_LENGTH - maximum length of a bucket. If an 
 * element must be added to a bucket that already contains the maximum
 * number of elements, the oldest element is removed from the bucket. */
#define KEDR_TINDEX_TABLE_HASH_BITS   10
#define KEDR_TINDEX_TABLE_SIZE   (1 << KEDR_TINDEX_TABLE_HASH_BITS)
#define KEDR_TINDEX_TABLE_BUCKET_LENGTH   KEDR_SAMPLING_NUM_TIDS

struct kedr_tindex_info
{
	/* The next element in the bucket, NULL for the last element. */
	struct kedr_tindex_info *next; 
	
	/* The ID assigned to the thread, see kedr_get_thread_id(). */
	unsigned long tid;
	
	/* The index assigned to the thread. */
	unsigned long tindex;	
};

/* The hash map {thread ID, tindex}.
 * Reading a bucket from this table requries rcu_read_lock/unlock.
 * Reading *(tindex_table[i]) requires rcu_dereference(). 
 *
 * If it is the first appearance of the given thread in the target module,
 * the corresponding bucket should be copied as a whole, the copy should be 
 * updated and then set instead of the old one, all this under 'upd_lock'. 
 * The stale buckets should be deleted by RCU callbacks. 
 *
 * Note that an entry for a thread can only be added in the table by that
 * very thread. */
static struct kedr_tindex_info **tindex_table = NULL;

/* The base index to assign to use for the next thread when it comes. 
 * (next_tindex + KEDR_SAMPLING_NUM_TIDS_IRQ) will be the index of that 
 * coming thread.
 * Should be used only with 'upd_lock' locked. */
static unsigned long next_tindex = 0;

int
kedr_init_tid_sampling(void)
{
	if (sampling_rate == 0)
		return 0;
	
	tindex_table = kzalloc(KEDR_TINDEX_TABLE_SIZE * 
		sizeof(struct kedr_tindex_info *), GFP_KERNEL);
	if (tindex_table == NULL)
		return -ENOMEM;
	
	/* Initially, the buckets are empty. */
	return 0;
}

/* Deletes the bucket starting with the given item. */
static void
tid_delete_bucket(struct kedr_tindex_info *item)
{
	struct kedr_tindex_info *next;
	while (item != NULL) {
		next = item->next;
		kfree(item);
		item = next;
	}
}

void
kedr_cleanup_tid_sampling(void)
{
	int i;
	if (sampling_rate == 0)
		return;
	
	BUG_ON(tindex_table == NULL);
	
	/* Wait until all our RCU callbacks have completed. */
	rcu_barrier();
	
	for (i = 0; i < KEDR_TINDEX_TABLE_SIZE; ++i) {
		tid_delete_bucket(tindex_table[i]);
	}
	kfree(tindex_table);
}

/* Returns 'tindex' for the given thread ID if this thread is known, i.e. it
 * entered the target some time ago and was not "forgotten" after that due
 * to the bucket being full. (unsigned long)(-1) is returned otherwise. 
 *
 * [NB] The situation where the 'tindex_info' for a live thread is removed 
 * from the table (because the bucket is already full) and is then inserted
 * back later is possible but seems to be unlikely. If that happens though,
 * performance overhead will increase as the update path will trigger more 
 * often. In addition, the sampling counters will become even less local to
 * the threads as the new 'tindex' assigned to the thread will not 
 * necessarily be the same as the old one. Should not be fatal though. */
static unsigned long
tid_lookup_index(unsigned long tid)
{
	unsigned long idx;
	unsigned long tindex;
	struct kedr_tindex_info *info;
	struct kedr_tindex_info *next;
	
	idx = hash_long(tid, KEDR_TINDEX_TABLE_HASH_BITS);
	
	rcu_read_lock();
	info = rcu_dereference(tindex_table[idx]);
	
	while (info != NULL) {
		next = info->next;
		if (info->tid == tid)
			break;
		info = next;
	}
	
	if (info == NULL) {
		rcu_read_unlock();
		return (unsigned long)(-1);
	}
	
	tindex = info->tindex;
	rcu_read_unlock();
	/* Now the bucket may go away, if needed. */
	
	return tindex;
}

/* Creates an updated copy of the given bucket. Returns the pointer to the
 * created bucket if successful, NULL if there is not enough memory. 
 *
 * [NB] 'old_bucket' is NULL if this is the first time something should be 
 * placed in this bucket.
 * 
 * The information about the thread with the specified TID and 'tindex' is
 * added to the created bucket. The oldest item in the bucket may be evicted
 * if the bucket is already full. 
 *
 * Should be called with 'upd_lock' locked. */
static struct kedr_tindex_info *
tid_create_new_bucket(const struct kedr_tindex_info *old_bucket, 
	unsigned long tid, unsigned long tindex)
{
	struct kedr_tindex_info *item;
	const struct kedr_tindex_info *old_item;
	struct kedr_tindex_info *new_bucket;
	int len;
	
	item = kzalloc(sizeof(*item), GFP_ATOMIC);
	if (item == NULL)
		return NULL;
	
	item->tid = tid;
	item->tindex = tindex;	
	new_bucket = item;
	len = 1;
	old_item = old_bucket;
	
	while (old_item != NULL && len < KEDR_TINDEX_TABLE_BUCKET_LENGTH) {
		struct kedr_tindex_info *new_item;
		new_item = kzalloc(sizeof(*new_item), GFP_ATOMIC);
		if (new_item == NULL) {
			tid_delete_bucket(new_bucket);
			return NULL;
		}
	
		new_item->tid = old_item->tid;
		new_item->tindex = old_item->tindex;
		item->next = new_item;
		++len;
		
		item = new_item;
		old_item = old_item->next;
	}
	return new_bucket;
}

struct kedr_tindex_bucket_rcu
{
	struct rcu_head rcu;
	struct kedr_tindex_info *bucket;
};

static void 
reclaim_tindex_bucket(struct rcu_head *rp)
{
	struct kedr_tindex_bucket_rcu *p = 
		container_of(rp, struct kedr_tindex_bucket_rcu, rcu);
	tid_delete_bucket(p->bucket);
	kfree(p);
}

long
kedr_get_tindex(void)
{
	unsigned long tid;
	unsigned long tindex;
	unsigned long irq_flags;
	unsigned long idx;
	struct kedr_tindex_info *old_bucket;
	struct kedr_tindex_info *new_bucket;
	struct kedr_tindex_bucket_rcu *tbr = NULL;
	
	if (sampling_rate == 0)
		return 0;
	
	if (in_interrupt())
		return ((unsigned long)smp_processor_id() % 
			KEDR_SAMPLING_NUM_TIDS_IRQ);
	
	/* OK, if we've got here, it is a "normal thread". 
	* Check if its 'tindex' is already known. */
	tid = (unsigned long)current;
	tindex = tid_lookup_index(tid);
	if (tindex != (unsigned long)(-1))
		return (long)tindex;
	
	/* The thread is new to the target module, go the slow path and 
	 * assign 'tindex' to that thread. Note that no other thread could
	 * have added an entry for the same TID after the call to 
	 * tid_lookup_index() (TID is the address if the task struct for the
	 * thread. No other thread can have the same address of the task 
	 * struct while the thread in question is running). So, there is no 
	 * race window for this TID here. */
	idx = hash_long(tid, KEDR_TINDEX_TABLE_HASH_BITS);
	spin_lock_irqsave(&upd_lock, irq_flags);
	
	old_bucket = tindex_table[idx];
	tindex = next_tindex + KEDR_SAMPLING_NUM_TIDS_IRQ;
	
	if (old_bucket != NULL) {
		tbr = kzalloc(sizeof(*tbr), GFP_ATOMIC);
		if (tbr == NULL) {
			spin_unlock_irqrestore(&upd_lock, irq_flags);
			return -ENOMEM;
		}
		tbr->bucket = old_bucket;
	}
	
	new_bucket = tid_create_new_bucket(old_bucket, tid, tindex);
	if (new_bucket == NULL) {
		spin_unlock_irqrestore(&upd_lock, irq_flags);
		kfree(tbr);
		return -ENOMEM;
	}
	rcu_assign_pointer(tindex_table[idx], new_bucket);
	
	/* Schedule deletion of the old bucket */
	if (tbr != NULL)
		call_rcu(&tbr->rcu, reclaim_tindex_bucket);
	
	next_tindex = (next_tindex + 1) % KEDR_SAMPLING_NUM_TIDS;
	spin_unlock_irqrestore(&upd_lock, irq_flags);
	
	return (long)tindex;
}
/* ====================================================================== */
