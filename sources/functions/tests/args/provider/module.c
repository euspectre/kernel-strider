/* This module provides (exports) a test function to be called and analyzed
 * by other modules. */

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

#include "test_arg.h"
/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

unsigned long 
kedr_test_arg_func(unsigned long arg1, unsigned long arg2, 
	unsigned long arg3, unsigned long arg4, unsigned long arg5,
	unsigned long arg6, unsigned long arg7, unsigned long arg8)
{
	arg8 /= 4;
	if (arg8 == arg1 && 
	   (arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7) == 0) {
		/* This should never happen during testing. It is only 
		 * needed to make the compiler think the change in 'arg8'
		 * is really used by some code here. The compiler must not
		 * optimize it away. In addition, make it look as if other 
		 * arguments are used too. */
		return 0;
	}
	return (unsigned long)&kedr_test_arg_func;
}
EXPORT_SYMBOL(kedr_test_arg_func);
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	return;
}

static int __init
test_init_module(void)
{
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
