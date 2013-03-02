/* This module provides the pre-/post- handlers for the calls to the test
 * function(s) and checks that the arguments and the return value of the 
 * callee(s) can be correctly retrieved. 
 *
 * If the test implemented here has not run or has detected an error, 
 * 'test_failed' parameter will remain non-zero. Otherwise, the parameter
 * will be 0. */

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
#include <linux/rcupdate.h>
#include <linux/spinlock.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>

#include "test_cbh.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[test_cbh_checker] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Non-zero - the corresponding part of the test passed, 0 - failed. */
int first_pre_ok = 0;
module_param(first_pre_ok, int, S_IRUGO);

int first_post_ok = 0;
module_param(first_post_ok, int, S_IRUGO);

int second_pre_ok = 0;
module_param(second_pre_ok, int, S_IRUGO);

int second_post_ok = 0;
module_param(second_post_ok, int, S_IRUGO);
/* ====================================================================== */

static unsigned long
get_arg(struct kedr_local_storage *ls, int n)
{
	switch (n) {
	case 1:
		return KEDR_LS_ARG1(ls);
	case 2:
		return KEDR_LS_ARG2(ls);
	case 3:
		return KEDR_LS_ARG3(ls);
	case 4:
		return KEDR_LS_ARG4(ls);
	case 5:
		return KEDR_LS_ARG5(ls);
	case 6:
		return KEDR_LS_ARG6(ls);
	case 7:
		return KEDR_LS_ARG7(ls);
	case 8:
		return KEDR_LS_ARG8(ls);
	default:
		BUG(); /* should not get here */
		return 0;
	}
}

static unsigned long expected_args[] = {
	KEDR_TEST_ARG1, KEDR_TEST_ARG2, KEDR_TEST_ARG3, KEDR_TEST_ARG4,
	KEDR_TEST_ARG5, KEDR_TEST_ARG6, KEDR_TEST_ARG7, KEDR_TEST_ARG8
};

#define KEDR_TEST_ARGS_TOTAL 8

/* KEDR_TEST_ARGS_REG - the number of parameters passed in the registers. */
#ifdef CONFIG_X86_64
#define KEDR_TEST_ARGS_REG 6
#else /* CONFIG_X86_32 */
#define KEDR_TEST_ARGS_REG 3
#endif

static int 
check_args_pre(struct kedr_local_storage *ls)
{
	unsigned long arg;
	int i;
	
	for (i = 1; i <= KEDR_TEST_ARGS_TOTAL; ++i) {
		arg = get_arg(ls, i);
		if (arg != expected_args[i - 1]) {
			pr_warning(KEDR_MSG_PREFIX 
			"Mismatch in check_args_pre(): "
			"the argument #%d is 0x%lx (should be 0x%lx)\n",
				i, arg, expected_args[i - 1]);
			return 1;
		}
	}
	return 0;
}

static int 
check_args_post(struct kedr_local_storage *ls)
{
	/* [NB] Only the parameters passed in the registers are 
	 * guaranteed to have the same value both in the pre- and in the 
	 * post-handler (they are saved before the pre-handler is called
	 * and therefore, before the target is called). */
	unsigned long arg;
	int i;
	
	for (i = 1; i <= KEDR_TEST_ARGS_REG; ++i) {
		arg = get_arg(ls, i);
		if (arg != expected_args[i - 1]) {
			pr_warning(KEDR_MSG_PREFIX 
			"Mismatch in check_args_post(): "
			"the argument #%d is 0x%lx (should be 0x%lx)\n",
				i, arg, expected_args[i - 1]);
			return 1;
		}
	}
	return 0;
}

static void 
test_first_pre(struct kedr_local_storage *ls)
{
	void *data;
	
	data = rcu_dereference(ls->fi->data);
	if (data == (void *)THIS_MODULE) {
		first_pre_ok = 1;
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
		"test_first_pre(): data should be %p but it is %p\n",
			(void *)THIS_MODULE, data);
	}
}

static void 
test_first_post(struct kedr_local_storage *ls)
{
	void *data;
	
	data = rcu_dereference(ls->fi->data);
	if (data == (void *)THIS_MODULE) {
		first_post_ok = 1;
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
		"test_first_post(): data should be %p but it is %p\n",
			(void *)THIS_MODULE, data);
	}
}

static void 
test_second_pre(struct kedr_local_storage *ls)
{
	void *data;
	
	if (check_args_pre(ls) != 0)
		return;
	
	/* Save the argument #8 in the local storage for later use in the
	 * post-handler. This argument is passed on stack both on x86-32
	 * and on x86-64. */
	ls->data = KEDR_LS_ARG8(ls);
	
	data = rcu_dereference(ls->fi->data);
	if (data != (void *)&first_post_ok) {
		pr_warning(KEDR_MSG_PREFIX 
		"test_second_pre(): data should be %p but it is %p\n",
			(void *)&first_post_ok, data);
		return;
	}
	
	/* This part of the test has passed. */
	second_pre_ok = 1;
}

static void 
test_second_post(struct kedr_local_storage *ls)
{
	void *data;
	unsigned long ret_val = 0;

	if (check_args_post(ls) != 0)
		return;
	
	if (ls->data != KEDR_TEST_ARG8) {
		pr_warning(KEDR_MSG_PREFIX 
		"test_second_post(): the saved value of the argument #8 "
		"(0x%lx) differs from the expected one (0x%lx).\n",
			ls->data, (unsigned long)KEDR_TEST_ARG8);
		return;
	}
	
	ret_val = KEDR_LS_RET_VAL(ls);
	if (ret_val != ls->fi->addr) {
		pr_warning(KEDR_MSG_PREFIX 
		"test_second_post(): the return value (0x%lx) "
		"differs from the expected one (0x%lx).\n",
			ret_val, ls->fi->addr);
		return;
	}
	
	data = rcu_dereference(ls->fi->data);
	if (data != (void *)&first_post_ok) {
		pr_warning(KEDR_MSG_PREFIX 
		"test_second_pre(): data should be %p but it is %p\n",
			(void *)&first_post_ok, data);
		return;
	}
	
	second_post_ok = 1;
}
/* ====================================================================== */

static void 
set_handlers_for_callback(struct kedr_func_info *fi, 
	void (*pre_handler)(struct kedr_local_storage *),
	void (*post_handler)(struct kedr_local_storage *),
	void *data)
{
	unsigned long flags;
	
	spin_lock_irqsave(&fi->handler_lock, flags);
	if (fi->pre_handler == NULL)
		rcu_assign_pointer(fi->pre_handler, pre_handler);
	if (fi->post_handler == NULL)
		rcu_assign_pointer(fi->post_handler, post_handler);
	
	fi->data = data;
	spin_unlock_irqrestore(&fi->handler_lock, flags);
}

static void 
test_pre(struct kedr_local_storage *ls)
{
	struct kedr_test_cbh_ops *cbh_ops;
	struct kedr_func_info *fi;
	
	/* The structure containing the list of the callbacks is the first
	 * and the only argument of kedr_test_cbh_register(). */
	cbh_ops = (struct kedr_test_cbh_ops *)KEDR_LS_ARG1(ls);
	
	if (cbh_ops->first != NULL) {
		fi = kedr_find_func_info((unsigned long)cbh_ops->first);
		if (fi != NULL)
			set_handlers_for_callback(fi, 
				test_first_pre, test_first_post,
				THIS_MODULE);
	}
	
	if (cbh_ops->second != NULL) {
		fi = kedr_find_func_info((unsigned long)cbh_ops->second);
		if (fi != NULL)
			set_handlers_for_callback(fi, 
				test_second_pre, test_second_post,
				&first_post_ok);
	}
}

static void 
test_post(struct kedr_local_storage *ls)
{
	/* This handler is intentionally empty. */
}

static struct kedr_fh_handlers handlers = {
	.orig = &kedr_test_cbh_register,
	.pre = test_pre,
	.post = test_post,
};

static struct kedr_fh_handlers *handlers_array[] = {
	&handlers, 
	NULL
};

static struct kedr_fh_plugin fh = {
	.owner = THIS_MODULE,
	.handlers = &handlers_array[0],
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_fh_plugin_unregister(&fh);
}

static int __init
test_init_module(void)
{
	BUILD_BUG_ON(ARRAY_SIZE(expected_args) != KEDR_TEST_ARGS_TOTAL);
	return kedr_fh_plugin_register(&fh);
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
