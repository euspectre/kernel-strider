/* A module to test allocation and deallocation of memory in the 
 * module mapping space. */

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
#include <linux/string.h>

#include "config.h"
#include "core_impl.h"
#include "module_ms_alloc.h"

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* "test_failed" - test result, 0 - passed, any other value - failed.
 * Default: failed. */
int test_failed = 1; 
module_param(test_failed, int, S_IRUGO);
/* ====================================================================== */

/* The amount of memory to request, in bytes. */
#define KEDR_TEST_MEM_SIZE 4096
/* ====================================================================== */

/* Check if the distance between 'ptr' and the reference point in the module
 * mapping space is less than 2^31. We neglect the cases when it can be 
 * equal to 2^31. */
static int
distance_ok(void *ptr, void *ref_point)
{
	u64 pt = (u64)(unsigned long)ptr;
	u64 ref_pt = (u64)(unsigned long)ref_point;
	u64 diff;
	
	diff = (pt >= ref_pt) ? (pt - ref_pt) : (ref_pt - pt);
	return (diff < (u64)0x80000000);
}

static void
do_test(void)
{
	unsigned char *p1 = NULL;
	unsigned char *p2 = NULL;
	int i; 
	
	p1 = kedr_module_alloc(KEDR_TEST_MEM_SIZE);
	if (p1 == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"Failed to allocate memory block #1.\n");
		return;
	}
	
	p2 = kedr_module_alloc(KEDR_TEST_MEM_SIZE);
	if (p2 == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"Failed to allocate memory block #2.\n");
		return;
	}
	
	/* Use the allocated memory somehow to check if it is usable. */
	memset(p1, 0xcc, KEDR_TEST_MEM_SIZE);
	memset(p2, 0, KEDR_TEST_MEM_SIZE);
	
	for (i = 0; i < KEDR_TEST_MEM_SIZE; ++i) {
		if (p1[i] != 0xcc || p2[i] != 0) {
			pr_warning(KEDR_MSG_PREFIX
				"The allocated memory is unusable.\n");
			goto out;
		}
	}
	
	if (!distance_ok(p1, (void *)&do_test) || 
	    !distance_ok(p2, (void *)&do_test)) {
	    	pr_warning(KEDR_MSG_PREFIX
	"The allocated memory is not in the module mapping space.\n");
			goto out;
	}
	
	kedr_module_free(p1);
	kedr_module_free(p2);
	
	/* kedr_module_free(NULL) should be a no-op. We need, at least, to 
	 * call that function with NULL as an argument. */
	kedr_module_free(NULL);
	
	test_failed = 0;
	return;
	
out:
	kedr_module_free(p1);
	kedr_module_free(p2);
	return;
}
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_cleanup_module_ms_alloc();
	return;
}

static int __init
test_init_module(void)
{
	int ret = 0;
	
	ret = kedr_init_module_ms_alloc();
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
		"Failed to initialize the allocation subsystem.\n");
		return ret;
	}
	
	do_test();
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
