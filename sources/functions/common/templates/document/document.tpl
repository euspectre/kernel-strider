/* This module is responsible for calling appropriate pre and post handlers
 * for some of the functions called by the target module, namely: 
 * lock/unlock operations, alloc/free, and may be some more. 
 *
 * The focus here is on the functions that could be interesting when 
 * detecting data races, hence "drd" in the names.
 * 
 * See also on_*_pre() / on_*_post() in core_api.h.
 *
 * No replacement is provided here for the target functions, so the latter
 * should execute as they are. */
	
/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Author: 
 *      Eugene A. Shatokhin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/percpu.h>
#include <linux/hash.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>

#include "config.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */
	
#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

#ifndef __percpu
/* This "specifier" was introduced in the kernel 2.6.33. Dynamically
 * allocated per-CPU variables were declared without it in 2.6.32. To make
 * the code uniform, define __percpu here. */
#define __percpu
#endif

/* Per-CPU IDs of happens-before relations for BH- and IRQ-related stuff.
 *
 * IRQ handlers are considered to belong to their CPU-specific threads by
 * KernelStrider, different from the threads that were interrupted.
 * BH functions (timer/tasklet/softirq operations) can also execute in
 * their dedicated threads.
 * 
 * Suppose a regular thread executes a section with IRQ or BH disabled (note
 * that they are disabled on the local CPU only) and accesses some data.
 * Suppose an IRQ handler or BH function kicks in (on that CPU) after that
 * section completes and that function accesses the same data.
 * Our system would see two threads, the regular one and the one for IRQ/BH,
 * accessing the same data without any synchronization and would report a
 * race. If the data could be accessed not only from the code on this CPU,
 * that would indeed be a race but for the "CPU-bound" data that would
 * actually be a false positive.
 *
 * 'kedr_bh_irq_id' are used to express the happend-before relations that
 * arise here to avoid such false positives.
 *
 * The relations between the sections of type "something" and the sections
 * where that "something" is disabled are symmetrical: we assume here the
 * sections of one kind can execute concurrently with each other but the
 * sections of different kinds can not. Note that this is only for the
 * current CPU only, the sections of different kinds are allowed to execute
 * concurrently on different CPUs.
 *
 * On a given CPU, this can be expressed as follows (id is CPU-specific):
 * ["something"]		["something disabled"]
 * happens-after(id)		happens-after(id)
 * <code>			<code>
 * happens-before(id)		happens-before(id)
 *
 * Note that if that did not affect a given CPU only, at least 2 IDs would
 * be needed to express the relation properly and not to imply that
 * concurrent execution of "something" sections is not possible on different
 * CPUs. We assume that the sections of the same kind cannot interrupt each
 * other.
 *
 * The IDs corresponding to kedr_bh_irq_id are used for IRQ-related
 * relations, to get the IDs for BH-related code, add 1 to these IDs.
 *
 * For a BH function we also assume that it never executes concurrently with
 * itself on different CPUs. This is the case for timers and tasklets but
 * may not be so for softirqs (see "Unreliable Guide to Locking"). We
 * simplify things a bit here, assuming the rule is the same for all BH
 * functions. May lose some races as a result in case of softirqs but that
 * should be unlikely.
 *
 * This is expressed as a happens-before relation with the address of the
 * BH function as the ID.
 * 
 * Different BH functions, on the other hand, can execute concurrently on
 * different CPUs.
 *
 * IRQ handlers (the same or different ones) also can execute concurrently
 * on different CPUs.
 * ------------------------------------------------------------------------
 * 
 * Contexts and assumptions:
 * BH+/- and IRQ+/- mean what is enabled (+) or disabled (-) on the local
 * CPU.
 * 
 * 1. Process
 * 	regular		BH+, IRQ+
 * 	BH disabled	BH-, IRQ+
 * 	IRQ disabled	BH-, IRQ-
 *
 * 2. BH
 * 	regular		BH-, IRQ+
 * 	IRQ disabled	BH-, IRQ-
 *
 * 3. IRQ		BH-, IRQ-
 * ------------------------------------------------------------------------
 */
unsigned long __percpu *kedr_bh_irq_id;
/* ====================================================================== */

/* IDs of the particular happens-before arcs. */
/* The arcs involving the system-wide workqueues. */
unsigned long kedr_system_wq_id = 0;
/* ====================================================================== */

/* The list of function groups. */
static LIST_HEAD(groups);
/* ====================================================================== */

<$declare_group : join(\n)$>
/* ====================================================================== */

static void
on_init_post(struct kedr_fh_plugin *fh, struct module *mod,
	     void **per_target)
{
	unsigned long tid;
	unsigned long pc;
	unsigned long *id_ptr = (unsigned long *)per_target;

	/* ID of a happens-before arc from the end of the init function to
	 * the beginning of the exit function for a given target). 
	 * The function is executed in an atomic context, namely, under RCU
	 * read-side lock, so we cannot use kedr_get_unique_id(). We use
	 * the address of the struct module instead. 
	 * TODO: take this into account when revisiting other components 
	 * that track module get/put, etc., to avoid ID collisions. */
	*id_ptr = (unsigned long)mod;
	
	/* Specify the relation "init happens-before cleanup" */
	tid = kedr_get_thread_id();
	pc = (unsigned long)mod->init;
	
	kedr_happens_before(tid, pc, *id_ptr);
}

static void
on_exit_pre(struct kedr_fh_plugin *fh, struct module *mod,
	    void **per_target)
{
	unsigned long tid;
	unsigned long pc;
	unsigned long *id_ptr = (unsigned long *)per_target;

	if (*id_ptr == 0) {
		pr_warning(KEDR_MSG_PREFIX "on_exit_pre(): "
	"failed to find ID of init-exit happens-before arc for %s.",
			module_name(mod));
		return;
	}	
	
	/* Specify the relation "init happens-before cleanup" */
	tid = kedr_get_thread_id();
	pc = (unsigned long)mod->exit;
	
	kedr_happens_after(tid, pc, *id_ptr);
}

static struct kedr_fh_plugin fh = {
	.owner = THIS_MODULE,
	.on_init_post = on_init_post,
	.on_exit_pre = on_exit_pre,
};
/* ====================================================================== */

static int
create_per_cpu_ids(void)
{
	int ret = 0;
	unsigned int cpu;
	
	kedr_bh_irq_id = alloc_percpu(unsigned long);
	if (kedr_bh_irq_id == NULL)
		return -ENOMEM;
	
	for_each_possible_cpu(cpu) {
		unsigned long *p;
		p = per_cpu_ptr(kedr_bh_irq_id, cpu);
		*p = kedr_get_unique_id();
		if (*p == 0) {
			ret = -ENOMEM;
		}
	}
	if (ret != 0)
		free_percpu(kedr_bh_irq_id);

	return ret;
}

static void
free_per_cpu_ids(void)
{
	free_percpu(kedr_bh_irq_id);
}

static unsigned long
get_per_cpu_id(void)
{
	unsigned long *ptr = NULL;
	unsigned long id;
	unsigned long cpu = get_cpu();
	ptr = per_cpu_ptr(kedr_bh_irq_id, cpu);
	id = *ptr;
	put_cpu();
	return id;
}
/* ====================================================================== */
	
void
kedr_bh_start(unsigned long tid, unsigned long func)
{
	unsigned long id = get_per_cpu_id() + 1;

	/* A BH function cannot be executed on 2 or more CPUs at the same 
	 * time. */
	kedr_happens_after(tid, func, func);
	
	/* BH VS BH disabled. */
	kedr_happens_after(tid, func, id);
}
EXPORT_SYMBOL(kedr_bh_start);

void
kedr_bh_end(unsigned long tid, unsigned long func)
{
	unsigned long id = get_per_cpu_id() + 1;

	/* BH VS BH disabled. */
	kedr_happens_before(tid, func, id);
	
	/* A BH function cannot be executed on 2 or more CPUs at the same 
	 * time. */
	kedr_happens_before(tid, func, func);
}
EXPORT_SYMBOL(kedr_bh_end);

void
kedr_bh_disabled_start(unsigned long tid, unsigned long pc)
{
	unsigned long id = get_per_cpu_id() + 1;
	kedr_happens_after(tid, pc, id);
}
EXPORT_SYMBOL(kedr_bh_disabled_start);

void
kedr_bh_disabled_end(unsigned long tid, unsigned long pc)
{
	unsigned long id = get_per_cpu_id() + 1;
	kedr_happens_before(tid, pc, id);
}
EXPORT_SYMBOL(kedr_bh_disabled_end);

void
kedr_irq_start(unsigned long tid, unsigned long func)
{
	unsigned long id = get_per_cpu_id();
	kedr_bh_disabled_start(tid, func);
	
	/* IRQ VS IRQ disabled. */
	kedr_happens_after(tid, func, id);
}
EXPORT_SYMBOL(kedr_irq_start);

void
kedr_irq_end(unsigned long tid, unsigned long func)
{
	unsigned long id = get_per_cpu_id();
	
	/* IRQ VS IRQ disabled. */
	kedr_happens_before(tid, func, id);
	kedr_bh_disabled_end(tid, func);
}
EXPORT_SYMBOL(kedr_irq_end);

void
kedr_irq_disabled_start(unsigned long tid, unsigned long pc)
{
	unsigned long id = get_per_cpu_id();
	kedr_bh_disabled_start(tid, pc);
	
	/* IRQ VS IRQ disabled. */
	kedr_happens_after(tid, pc, id);
}
EXPORT_SYMBOL(kedr_irq_disabled_start);

void
kedr_irq_disabled_end(unsigned long tid, unsigned long pc)
{
	unsigned long id = get_per_cpu_id();
	
	/* IRQ VS IRQ disabled. */
	kedr_happens_before(tid, pc, id);
	kedr_bh_disabled_end(tid, pc);
}
EXPORT_SYMBOL(kedr_irq_disabled_end);
/* ====================================================================== */

/* Parameters of the hash table containing the information about the 
 * currently taken locks. */
#define KEDR_FH_LOCK_TABLE_BITS 8
#define KEDR_FH_LOCK_TABLE_SIZE (1 << KEDR_FH_LOCK_TABLE_BITS)

/* Maximum length of the bucket allowed. If an element should be added to a
 * bucket which is already full, the oldest element of the bucket will be 
 * evicted, a warning will be issued and the new element will be added. 
 * This is needed to avoid hogging memory on the systems where we cannot 
 * track some of the unlock operations (for example, if spin_unlock() is an
 * inline). The results reported by KernelStrider will be unreliable in this 
 * case but at least it must not cause problems to the OS. */
#define KEDR_FH_LOCK_TABLE_MAX_BUCKET 128

struct kedr_lock_item
{
	struct kedr_lock_item *next;
	unsigned long lock_id;
};

/* The table and the spinlock to protect it. */
static struct kedr_lock_item **lock_table;
static DEFINE_SPINLOCK(table_lock);

static void
warn_once_bucket_full(unsigned long pc)
{
	static int warned = 0;
	if (warned)
		return;
	
	warned = 1;
	pr_warning(KEDR_MSG_PREFIX 
		"A bucket in the lock table was already full (%u items) "
		"before the lock taken at %p was added - "
		"some unlock operations were not detected?\n",
		(unsigned int)KEDR_FH_LOCK_TABLE_MAX_BUCKET,
		(void *)pc);
}

static void
evict_oldest(struct kedr_lock_item **bucket)
{
	struct kedr_lock_item *it;
	struct kedr_lock_item **pnext = bucket;
	
	if (*bucket == NULL)
		return;
	
	for (it = *bucket; it->next != NULL; it = it->next) {
		pnext = &it->next;
	}
	*pnext = NULL;
	kfree(it);
}

int
kedr_fh_mark_locked(unsigned long pc, unsigned long lock_id)
{
	unsigned long flags;
	unsigned int length = 0;
	unsigned long bucket;
	struct kedr_lock_item *it;
	int new_lock = 1;
	
	bucket = hash_ptr((void *)lock_id, KEDR_FH_LOCK_TABLE_BITS);
	
	spin_lock_irqsave(&table_lock, flags);
	it = lock_table[bucket];
	while (it != NULL) {
		++length;
		if (it->lock_id == lock_id) {
			new_lock = 0;
			break;
		}
		it = it->next;
	}
	
	if (new_lock) {
		struct kedr_lock_item *new_item;
		if (length >= KEDR_FH_LOCK_TABLE_MAX_BUCKET) {
			warn_once_bucket_full(pc);
			evict_oldest(&lock_table[bucket]);
		}
		
		new_item = kzalloc(sizeof(*new_item), GFP_ATOMIC);
		if (new_item == NULL) {
			pr_warning(KEDR_MSG_PREFIX 
			"kedr_fh_mark_locked(): not enough memory.\n");
			spin_unlock_irqrestore(&table_lock, flags);
			return -ENOMEM;
		}
		
		/* Add the new item at the start of the bucket to make the
		 * search faster (LIFO scenario should be common for locks).
		 */
		it = lock_table[bucket];
		new_item->next = it;
		new_item->lock_id = lock_id;
		lock_table[bucket] = new_item;
	}
	
	spin_unlock_irqrestore(&table_lock, flags);
	return new_lock;
}
EXPORT_SYMBOL(kedr_fh_mark_locked);

void
kedr_fh_mark_unlocked(unsigned long pc, unsigned long lock_id)
{
	unsigned long flags;
	unsigned long bucket;
	struct kedr_lock_item *it;
	struct kedr_lock_item **pnext;
	
	bucket = hash_ptr((void *)lock_id, KEDR_FH_LOCK_TABLE_BITS);
	
	spin_lock_irqsave(&table_lock, flags);
	it = lock_table[bucket];
	pnext = &lock_table[bucket];
	while (it != NULL) {
		if (it->lock_id == lock_id)
			break;
		pnext = &it->next;
		it = it->next;
	}
	if (it != NULL) {
		/* Found the matching lock operation, OK, remove it. */
		*pnext = it->next;
		kfree(it);
	}
	/* There may be unlock operations we haven't track lock operations
	 * for. It is possible in case of read locks. Just ignore them. */
	
	spin_unlock_irqrestore(&table_lock, flags);
	return;
}
EXPORT_SYMBOL(kedr_fh_mark_unlocked);

static int
lock_table_create(void)
{
	lock_table = vmalloc(
		sizeof(lock_table[0]) * KEDR_FH_LOCK_TABLE_SIZE);
	if (lock_table == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to create the lock table.\n");
		return -ENOMEM;
	}
	
	memset(lock_table, 0, 
	       sizeof(lock_table[0]) * KEDR_FH_LOCK_TABLE_SIZE);
	return 0;
}

/* [NB] No need to protect the table here: this function should be called
 * when the plugin is being unloaded and no event in the target modules can 
 * interfere. */
static void
lock_table_destroy(void)
{
	unsigned int i;
	for (i = 0; i < KEDR_FH_LOCK_TABLE_SIZE; ++i) {
		struct kedr_lock_item *it = lock_table[i];
		while (it != NULL) {
			struct kedr_lock_item *next = it->next;
			kfree(it);
			it = next;
		}
	}
	vfree(lock_table);
}
/* ====================================================================== */

static int __init
func_drd_init_module(void)
{
	int ret;
	
	kedr_system_wq_id = kedr_get_unique_id();
	if (kedr_system_wq_id == 0) {
		pr_warning(KEDR_MSG_PREFIX 
"Failed to get a unique ID for HB arcs involving system-wide wqs.\n");
		return -ENOMEM;
	}
	
	ret = lock_table_create();
	if (ret != 0)
		goto err;
	
	ret = create_per_cpu_ids();
	if (ret != 0)
		goto err_per_cpu;
	
	/* Add the groups of functions to be handled. */
<$add_group : join(\n)$>
	
	fh.handlers = kedr_fh_combine_handlers(&groups);
	if (fh.handlers == NULL) {
		ret = -ENOMEM;
		goto err_handlers;
	}
	
	ret = kedr_fh_plugin_register(&fh);
	if (ret != 0)
		goto err_reg;
	return 0;

err_reg:
	kfree(fh.handlers);
err_handlers:
	free_per_cpu_ids();	
err_per_cpu:
	lock_table_destroy();
err:
	return ret;
}

static void __exit
func_drd_exit_module(void)
{
	kedr_fh_plugin_unregister(&fh);
	
	kedr_fh_do_cleanup_calls(&groups);
	kfree(fh.handlers);
	free_per_cpu_ids();
	lock_table_destroy();
	
	/* [NB] If additional cleanup is needed, do it here. */
	return;
}

module_init(func_drd_init_module);
module_exit(func_drd_exit_module);
/* ====================================================================== */
