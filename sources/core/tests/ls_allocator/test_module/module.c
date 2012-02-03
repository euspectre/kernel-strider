/* A module to test the API for the allocators of the local storage
 * instances. */

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
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

#include <kedr/kedr_mem/local_storage.h> 

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* "test_failed" - test result, 0 - passed, any other value - failed */
int test_failed = 0; 
module_param(test_failed, int, S_IRUGO);
/* ====================================================================== */

/* A spinlock to establish an atomic context. */
static DEFINE_SPINLOCK(test_lock);
/* ====================================================================== */

/* "NULL Allocator", a custom allocator that always fails. */
static struct kedr_local_storage *
null_alloc_ls(struct kedr_ls_allocator *al)
{
	return NULL;
}

static void 
null_free_ls(struct kedr_ls_allocator *al, 
	struct kedr_local_storage *ls)
{
	return;
}

static struct kedr_ls_allocator null_allocator = {
	.alloc_ls = null_alloc_ls,
	.free_ls  = null_free_ls,
};
/* ====================================================================== */

/* This test scenario is quite shallow, deeper tests should be developed in 
 * the future when all the components of the core are implemented.
 * For now, it is OK if the code of the test compiles and runs without 
 * problems.
 * 
 * 1. Get the default allocator which should be the current allocator at 
 *    the moment. 
 * 2. Use the default allocator in atomic context (namely, with a spinlock 
 *    taken).
 * 3. Set a custom allocator (null_allocator).
 * 4. Get the current allocator - must be the one set at the previous step.
 * 5. Reset the current allocator, get it and check if the default allocator
 *    is now current. */
static void
do_test(void)
{
	struct kedr_ls_allocator *default_al = NULL;
	struct kedr_local_storage *ls = NULL;
	unsigned long irq_flags;
	int is_zeroed = 1;
	
	test_failed = 1; /* failed by default */
	
	/* [1] */
	default_al = kedr_get_ls_allocator();
	if (default_al == NULL) {
		pr_warning("[kedr_test] "
			"kedr_get_ls_allocator() returned NULL\n");
		return;
	}
	if (default_al->alloc_ls == NULL) {
		pr_warning("[kedr_test] "
			"kedr_get_ls_allocator()->alloc_ls is NULL\n");
		return;
	}
	if (default_al->free_ls == NULL) {
		pr_warning("[kedr_test] "
			"kedr_get_ls_allocator()->free_ls is NULL\n");
		return;
	}
	
	/* [2] */
	spin_lock_irqsave(&test_lock, irq_flags);
	ls = default_al->alloc_ls(default_al);
	if (ls == NULL) {
		/* Unlikely but still, this is not an error. */
		pr_warning("[kedr_test] "
		"The default allocator failed to allocate memory.\n");
	}
	else {
		/* Check if the newly allocated local storage is zeroed. */
		unsigned char *p = (unsigned char *)ls;
		unsigned int i;
		for (i = 0; i < sizeof(struct kedr_local_storage); ++i) {
			if (p[i] != 0) {
				is_zeroed = 0;
				break;
			}
		}
	}

	default_al->free_ls(default_al, ls);
	spin_unlock_irqrestore(&test_lock, irq_flags);
	
	/* [NB] is_zeroed also remains 1 if the allocator has failed to 
	 * allocate memory, so it is OK in that case too. */
	if (!is_zeroed) {
		pr_warning("[kedr_test] "
		"The default allocator failed to zero memory.\n");
		return;
	}
	
	/* [3] */
	kedr_set_ls_allocator(&null_allocator);
	
	/* [4] */
	if (kedr_get_ls_allocator() != &null_allocator) {
		pr_warning("[kedr_test] "
		"The custom allocator was not set.\n");
		return;
	}
	if (null_allocator.alloc_ls != null_alloc_ls ||
	    null_allocator.free_ls  != null_free_ls) {
	    	pr_warning("[kedr_test] "
			"kedr_set_ls_allocator() has changed the methods "
			"of the allocator.\n");
		return;
	}
	
	/* [5] */
	kedr_set_ls_allocator(NULL);
	if (kedr_get_ls_allocator() != default_al) {
		pr_warning("[kedr_test] "
		"The custom allocator was not reset.\n");
		return;
	}
	
	test_failed = 0;
}
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	return;
}

static int __init
test_init_module(void)
{
	do_test();
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
