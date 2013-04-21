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
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin
 *      Andrey V. Tsyvarev
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
#include <linux/sort.h>
#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/percpu.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include "config.h"
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
<$if concat(header)$>
<$header: join(\n)$>
/* ====================================================================== */<$endif$>

#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

/* IDs of the particular happens-before arcs. */
/* The arcs involving the system-wide workqueues. */
unsigned long kedr_system_wq_id = 0;
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */
<$if concat(header.main)$>
<$header.main: join(\n)$>
/* ====================================================================== */
<$endif$>
<$handlerDecl: join(\n\n)$>
/* ====================================================================== */

<$block: join(\n)$>
/* ====================================================================== */

static struct kedr_fh_handlers *handlers[] = {
	<$if concat(handlerItem)$><$handlerItem: join(,\n\t)$>,
	<$endif$>NULL
};
/* ====================================================================== */

/* For some of the functions we need to process, we cannot take the address
 * explicitly. For example, a compiler may not allow to use '&memset' if 
 * "memset" function, "memset" macro and/or "memset" builtin co-exist for
 * the sake of optimization, etc. However, to properly process the 
 * situations where the function is actually called, we need the address
 * of that function. This module will lookup the addresses of such 
 * functions via "kallsyms" during the initialization (see below). 
 * Note that the instances of "struct kedr_fh_drd_to_lookup" must not
 * be used after this module has completed its initialization. */
struct kedr_fh_drd_to_lookup
{
	const char *name;
	struct kedr_fh_handlers *h;
};

/* [NB] The last ("NULL") element is here for the array not to be empty */
static struct kedr_fh_drd_to_lookup __initdata to_lookup[] = {
<$lookupItem: join(\n)$>	{NULL, NULL} 
};
/* ====================================================================== */

/* A comparator, a swap function and a search function for 'to_lookup[]' 
 * array. Needed only during module init. */
static int __init
to_lookup_compare(const void *lhs, const void *rhs)
{
	const struct kedr_fh_drd_to_lookup *left = 
		(const struct kedr_fh_drd_to_lookup *)lhs;
	const struct kedr_fh_drd_to_lookup *right = 
		(const struct kedr_fh_drd_to_lookup *)rhs;
	int result;
	
	BUILD_BUG_ON(ARRAY_SIZE(to_lookup) < 1);
	
	result = strcmp(left->name, right->name);
	if (result > 0)
		return 1;
	else if (result < 0)
		return -1;
	return 0;
}

static void __init
to_lookup_swap(void *lhs, void *rhs, int size)
{
	struct kedr_fh_drd_to_lookup *left = 
		(struct kedr_fh_drd_to_lookup *)lhs;
	struct kedr_fh_drd_to_lookup *right = 
		(struct kedr_fh_drd_to_lookup *)rhs;
	struct kedr_fh_drd_to_lookup t;
	
	BUG_ON(size != (int)sizeof(struct kedr_fh_drd_to_lookup));
	
	t.name = left->name;
	t.h = left->h;
	
	left->name = right->name;
	left->h = right->h;
	
	right->name = t.name;
	right->h = t.h;
}

/* Checks if 'to_lookup[]' contains a record for a function with the given
 * name. Returns the pointer to the record if found, NULL otherwise. 
 * 'to_lookup[]' must be sorted by function name in ascending order by this
 *  time */
static struct kedr_fh_drd_to_lookup * __init
to_lookup_search(const char *name)
{
	size_t beg = 0;
	size_t end = ARRAY_SIZE(to_lookup) - 1;
		
	BUILD_BUG_ON(ARRAY_SIZE(to_lookup) < 1);
	
	while (beg < end) {
		int result;
		size_t mid = beg + (end - beg) / 2;

		result = strcmp(name, to_lookup[mid].name);
		if (result < 0)
			end = mid;
		else if (result > 0)
			beg = mid + 1;
		else 
			return &to_lookup[mid];
	}
	return NULL;
}
/* ====================================================================== */

static void
on_init_post(struct kedr_fh_plugin *fh, struct module *mod,
	     void **per_target)
{
	unsigned long tid;
	unsigned long pc;
	unsigned long *id_ptr = (unsigned long *)per_target;

	/* ID of a happens-before arc from the end of the init function to
	 * the beginning of the exit function for a given target). */
	*id_ptr = kedr_get_unique_id();
	if (*id_ptr == 0) {
		pr_warning(KEDR_MSG_PREFIX "on_init_post(): "
	"failed to obtain ID of init-exit happens-before arc for %s.",
			module_name(mod));
		return;
	}

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

/* This function will be called for each symbol known to the system to 
 * find the addresses of the functions listed in 'to_lookup[]'.
 *
 * If this function returns 0, kallsyms_on_each_symbol() will continue
 * walking the symbols. If non-zero - it will stop. */
static int __init
symbol_walk_callback(void *data, const char *name, struct module *mod, 
	unsigned long addr)
{
	struct kedr_fh_drd_to_lookup *item;
	
	/* Skip the symbol if it belongs to a module rather than to 
	 * the kernel proper. */
	if (mod != NULL) 
		return 0;
	
	item = to_lookup_search(name);
	if (item == NULL)
		return 0;
	
	item->h->orig = (void *)addr;
	return 0;
}
/* ====================================================================== */
<$if concat(cleanup_func)$>
/* Group-specific cleanup functions. */
<$cleanupDecl: join(\n)$>
/* ====================================================================== */
<$endif$>
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

static int __init
func_drd_init_module(void)
{
	int ret = 0;
	size_t size = ARRAY_SIZE(to_lookup) - 1;
	size_t i;

	kedr_system_wq_id = kedr_get_unique_id();
	if (kedr_system_wq_id == 0) {
		pr_warning(KEDR_MSG_PREFIX 
"Failed to get a unique ID for HB arcs involving system-wide wqs.\n");
		return -ENOMEM;
	}

	/* Sort 'to_lookup[]' array (except the "NULL" element) in 
	 * ascending order by the function names. */
	sort(&to_lookup[0], size, sizeof(struct kedr_fh_drd_to_lookup),
		to_lookup_compare, to_lookup_swap);
	
	ret = kallsyms_on_each_symbol(symbol_walk_callback, NULL);
	if (ret != 0)
		return ret;
	
	/* Check that all the required addresses have been found. */
	for (i = 0; i < size; ++i) {
		if (to_lookup[i].h->orig == NULL) {
			pr_warning(KEDR_MSG_PREFIX 
			"Unable to find the address of %s function",
				to_lookup[i].name);
			return -EFAULT;
		}
	}

	ret = create_per_cpu_ids();
	if (ret != 0)
		return ret;
		
	fh.handlers = &handlers[0];
	ret = kedr_fh_plugin_register(&fh);
	if (ret != 0) {
		free_per_cpu_ids();
		return ret;
	}
	return 0;
}

static void __exit
func_drd_exit_module(void)
{
	kedr_fh_plugin_unregister(&fh);
	<$if concat(cleanup_func)$>
	<$cleanupCall: join(\n\t)$>
	<$endif$>
	
	free_per_cpu_ids();
	
	/* [NB] If additional cleanup is needed, do it here. */
	return;
}

module_init(func_drd_init_module);
module_exit(func_drd_exit_module);
/* ====================================================================== */
