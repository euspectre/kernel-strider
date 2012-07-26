/* This module is used when checking if the arguments can be correctly
 * retrieved in call pre-/post- handlers. */

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

#include <stdarg.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include "test_arg.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

static unsigned long
test_func(unsigned long arg1, unsigned long arg2, ...)
{
	va_list args;
	unsigned long ret;
	
	va_start(args, arg2);
	ret = kedr_test_arg_func_va_list(arg1, arg2, args);
	va_end(args);
	
	return ret;
}

static void __exit
test_cleanup_module(void)
{
	unsigned long ret;
	ret = kedr_test_arg_func(KEDR_TEST_ARG1, KEDR_TEST_ARG2, 
		KEDR_TEST_ARG3, KEDR_TEST_ARG4, KEDR_TEST_ARG5,
		KEDR_TEST_ARG6, KEDR_TEST_ARG7, KEDR_TEST_ARG8);
	BUG_ON(ret == 0);
	
	ret = kedr_test_arg_func_va(KEDR_TEST_ARG1, KEDR_TEST_ARG2, 
		KEDR_TEST_ARG3, KEDR_TEST_ARG4, KEDR_TEST_ARG5,
		KEDR_TEST_ARG6, KEDR_TEST_ARG7, KEDR_TEST_ARG8);
	BUG_ON(ret == 0);
	
	ret = test_func(KEDR_TEST_ARG1, KEDR_TEST_ARG2, 
		KEDR_TEST_ARG3, KEDR_TEST_ARG4, KEDR_TEST_ARG5,
		KEDR_TEST_ARG6, KEDR_TEST_ARG7, KEDR_TEST_ARG8);
	BUG_ON(ret == 0);
}

static int __init
test_init_module(void)
{
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
