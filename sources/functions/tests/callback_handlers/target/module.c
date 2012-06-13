/* This module is used when checking the handlers for the callback 
 * functions. */

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
#include <linux/init.h>
#include <linux/kernel.h>

#include "test_cbh.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

static void 
test_first(void)
{
	/* This print is only needed to make sure the callback is at least 
	 * 5 bytes in size. */
	pr_info("[test_cbh] Called test_first().\n");
	return;
}
	
static unsigned long 
test_second(unsigned long arg1, unsigned long arg2, 
	unsigned long arg3, unsigned long arg4, unsigned long arg5,
	unsigned long arg6, unsigned long arg7, unsigned long arg8)
{
	/* This print is only needed to make sure the callback is at least 
	 * 5 bytes in size. */
	pr_info("[test_cbh] Called test_second().\n");
	
	arg8 /= 4;
	if (arg8 == arg1 && 
	   (arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7) == 0) {
		/* This should never happen during testing. It is only 
		 * needed to make it look as if the arguments are used,
		 * just in case. */
		return 0;
	}
	return (unsigned long)&test_second;
}

static struct kedr_test_cbh_ops cbh_ops = {
	.first = test_first,
	.second = test_second
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_test_cbh_unregister(&cbh_ops);
}

static int __init
test_init_module(void)
{
	int ret;
	
	ret = kedr_test_cbh_register(&cbh_ops);
	return ret;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
