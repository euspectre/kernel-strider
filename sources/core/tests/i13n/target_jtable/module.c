/* A simple target module that is likely to contain jump tables in its 
 * binary code. The jump tables may be created by the compiler when 
 * optimizing switch statements, although it is not required. As the jump
 * tables are handled in a special way by the instrumentation system, a 
 * target module containing them is needed for testing. */

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
#include <linux/kernel.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* 'par1' and 'par2' are the parameters to be used as switch variables. */
int par1 = 0;
module_param(par1, int, S_IRUGO);
int par2 = 0;
module_param(par2, int, S_IRUGO);
/* ====================================================================== */

static void
do_one_switch(void)
{
	int r = 0;
	switch (par1) {
	case 0:
		r = 1;
		break;
	case 1:
		r = par2;
		break;
	case 2:
		r = par1 + 2 * par2;
		break;
	case 3:
		r = 3;
		break;
	case 4:
		r = par2;
		break;
	case 5:
		r = par1 + 6 * par2;
		break;
	case 6:
		r = 8;
		break;
	default:
		break;
	}
	
	/* Output 'r' to the log to make sure the compiler will never
	 * optimize the whole switch statement away. */
	pr_info("[target_jtable] "
		"(testing, ignore this message) result = %d\n", r);
}

static void
do_three_switches(void)
{
	int r1 = 0;
	int r2 = 0;
	
	switch (par1) {
	case 0:
		r1 = 3;
		break;
	case 1:
		r1 = par2;
		break;
	case 2:
		r1 = par1 + par2;
		break;
	case 3:
		r1 = 3;
		break;
	case 4:
		r1 = par1 + 1;
		break;
	case 5:
		r1 = par2 + 2;
		break;
	case 6:
		r1 = par2 + 2;
		break;
	case 7:
		r1 = par2 + 2;
		break;
	default:
		break;
	}
	
	switch (par2) {
	case 0:
		r2 = 2;
		break;
	case 1:
		r2 = par1;
		break;
	case 2:
		r2 = 2 * par1 - par2;
		break;
	case 3:
		r2 = 8;
		break;
	case 4:
		r2 = 5;
		break;
	case 5:
		r2 = par1 + 2;
		break;
	case 6:
		r2 = par1 * 2;
		break;
	case 7:
		r2 = par2 + 3;
		break;
	case 8:
		r2 = par1 * 2;
	default:
		break;
	}
	
	switch (par1 + par2) {
	case 0:
		r2 = r1;
		break;
	case 1:
		r1 = r2;
		break;
	case 2:
		r2 = r2 + 1;
		break;
	case 3:
		r1 = r1 + 1;
		break;
	case 4:
		r1 = r1 * 2;
		break;
	case 5:
		r1 = r1 / 2;
		break;
	case 6:
		r2 = r1 + 1;
		break;
	case 7:
		r2 = r1 * 3;
		break;
	case 8:
		r2 = r2 * r1;
		break;		
	case 9:
		r1 = r1 * r2;
		break;
	default:
		break;
	}
	
	/* Output 'r1' and 'r2' to the log to make sure the compiler will
	 * never optimize the whole switch statements away. */
	pr_info("[target_jtable] "
		"(testing, ignore this message) r1 = %d, r2 = %d\n",
		r1, r2);
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
	do_one_switch();
	do_three_switches();
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
