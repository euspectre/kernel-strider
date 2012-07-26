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
#include <linux/slab.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>

#include "test_arg.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[test_arg_checker] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Non-zero - test failed, 0 - test passed. */
int test_failed = 1;
module_param(test_failed, int, S_IRUGO);

/* 0 - check retrieval of the arguments and of the return value for an 
 * ordinary function;
 * 1 - same for a function with variable argument list; 
 * 2 - same for a function with 'va_list' as the last argument. */
unsigned int test_mode = 0;
module_param(test_mode, uint, S_IRUGO);
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

static unsigned long
get_arg_va(struct kedr_local_storage *ls, int n)
{
	switch (n) {
	case 1:
		return KEDR_LS_ARG1_VA(ls);
	case 2:
		return KEDR_LS_ARG2_VA(ls);
	case 3:
		return KEDR_LS_ARG3_VA(ls);
	case 4:
		return KEDR_LS_ARG4_VA(ls);
	case 5:
		return KEDR_LS_ARG5_VA(ls);
	case 6:
		return KEDR_LS_ARG6_VA(ls);
	case 7:
		return KEDR_LS_ARG7_VA(ls);
	case 8:
		return KEDR_LS_ARG8_VA(ls);
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
test_arg_func_pre(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	BUG_ON(info->target != (unsigned long)&kedr_test_arg_func);
	
	if (test_failed == 0) {
		test_failed = 1;
		pr_warning(KEDR_MSG_PREFIX 
		"test_arg_func_pre(): 'test_failed' is 0 on entry.\n");
		return;
	}
	
	if (check_args_pre(ls) != 0)
		return;
	
	/* Save the argument #8 in the local storage for later use in the
	 * post-handler. This argument is passed on stack both on x86-32
	 * and on x86-64. */
	ls->data = KEDR_LS_ARG8(ls);
	
	/* This part of the test has passed. */
	test_failed = 0;
}

static void 
test_arg_func_post(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long ret_val = 0;
	
	BUG_ON(info->target != (unsigned long)&kedr_test_arg_func);
	
	if (test_failed) 
		/* If the pre handler did not set it to 0, there was an
		 * error detected there. */
		return;
	
	/* Assume a failure by default. */
	test_failed = 1;
	if (check_args_post(ls) != 0)
		return;
	
	if (ls->data != KEDR_TEST_ARG8) {
		pr_warning(KEDR_MSG_PREFIX 
	"test_arg_func_post(): the saved value of the argument #8 (0x%lx) "
	"differs from the expected one (0x%lx).\n",
			ls->data, (unsigned long)KEDR_TEST_ARG8);
		return;
	}
	
	ret_val = KEDR_LS_RET_VAL(ls);
	if (ret_val != (unsigned long)&kedr_test_arg_func) {
		pr_warning(KEDR_MSG_PREFIX 
		"test_arg_func_post(): the return value (0x%lx) "
		"differs from the expected one (0x%lx).\n",
			ret_val, (unsigned long)&kedr_test_arg_func);
		return;
	}
	
	test_failed = 0;
}

static unsigned long *
save_args_va(struct kedr_local_storage *ls)
{
	unsigned long *saved_args;
	unsigned int i;
	
	saved_args = kzalloc(KEDR_TEST_ARGS_TOTAL * sizeof(unsigned long),
		GFP_ATOMIC);
	if (saved_args == NULL)
		return NULL;
	
	for (i = 1; i <= KEDR_TEST_ARGS_TOTAL; ++i)
		saved_args[i - 1] = get_arg_va(ls, i);
	return saved_args;
}

static int 
check_args_pre_va(struct kedr_local_storage *ls)
{
	unsigned long arg;
	int i;
	
	for (i = 1; i <= KEDR_TEST_ARGS_TOTAL; ++i) {
		arg = get_arg_va(ls, i);
		if (arg != expected_args[i - 1]) {
			pr_warning(KEDR_MSG_PREFIX 
			"Mismatch in check_args_pre_va(): "
			"the argument #%d is 0x%lx (should be 0x%lx)\n",
				i, arg, expected_args[i - 1]);
			return 1;
		}
	}
	return 0;
}

static int 
check_args_post_va(struct kedr_local_storage *ls)
{
	unsigned long arg;
	int i;
	unsigned long *saved_args = (unsigned long *)ls->data;
	
	for (i = 1; i <= KEDR_TEST_ARGS_TOTAL; ++i) {
		arg = saved_args[i - 1];
		if (arg != expected_args[i - 1]) {
			pr_warning(KEDR_MSG_PREFIX 
			"Mismatch in check_args_post_va(): "
			"the argument #%d is 0x%lx (should be 0x%lx)\n",
				i, arg, expected_args[i - 1]);
			return 1;
		}
	}
	return 0;
}

static void 
test_arg_func_pre_va(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	BUG_ON(info->target != (unsigned long)&kedr_test_arg_func_va);
	
	if (test_failed == 0) {
		test_failed = 1;
		pr_warning(KEDR_MSG_PREFIX 
		"test_arg_func_pre_va(): 'test_failed' is 0 on entry.\n");
		return;
	}
	
	if (check_args_pre_va(ls) != 0)
		return;
	
	/* Save the arguments for later use in the post-handler. */
	ls->data = (unsigned long)save_args_va(ls);
	if (ls->data == 0) {
		test_failed = 1;
		pr_warning(KEDR_MSG_PREFIX 
		"test_arg_func_pre_va(): failed to save the arguments.\n");
		return;
	}
	
	/* This part of the test has passed. */
	test_failed = 0;
}

static void 
test_arg_func_post_va(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)ls->info;
	unsigned long ret_val = 0;
	unsigned long *saved_args = (unsigned long *)ls->data;
	
	BUG_ON(info->target != (unsigned long)&kedr_test_arg_func_va);
	
	if (test_failed) 
		/* If the pre handler did not set it to 0, there was an
		 * error detected there. */
		return;
	
	/* Assume a failure by default. */
	test_failed = 1;
	
	if (ls->data == 0) {
		pr_warning(KEDR_MSG_PREFIX 
	"test_arg_func_post_va(): ls->data is 0 but it was expected to point "
	"to the saved argument values.\n");
		return;
	}
	
	if (check_args_post_va(ls) != 0)
		goto out;
	
	ret_val = KEDR_LS_RET_VAL(ls);
	if (ret_val != (unsigned long)&kedr_test_arg_func_va) {
		pr_warning(KEDR_MSG_PREFIX 
		"test_arg_func_post_va(): the return value (0x%lx) "
		"differs from the expected one (0x%lx).\n",
			ret_val, (unsigned long)&kedr_test_arg_func_va);
		goto out;
	}
	
	test_failed = 0;
out:
	kfree(saved_args);
}

static int 
check_args_va_list(struct kedr_local_storage *ls)
{
	unsigned long arg;
	int i;
	
	/* We need to check only the first 2 arguments. */
	for (i = 1; i <= 2; ++i) {
		arg = get_arg(ls, i);
		if (arg != expected_args[i - 1]) {
			pr_warning(KEDR_MSG_PREFIX 
			"Mismatch in check_args_va_list(): "
			"the argument #%d is 0x%lx (should be 0x%lx)\n",
				i, arg, expected_args[i - 1]);
			return 1;
		}
	}
	return 0;
}

static void 
test_arg_func_pre_va_list(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	BUG_ON(info->target != (unsigned long)&kedr_test_arg_func_va_list);
	
	if (test_failed == 0) {
		test_failed = 1;
		pr_warning(KEDR_MSG_PREFIX 
		"test_arg_func_pre_va_list(): 'test_failed' is 0 on entry.\n");
		return;
	}
	
	if (check_args_va_list(ls) != 0)
		return;
	
	/* This part of the test has passed. */
	test_failed = 0;
}

static void 
test_arg_func_post_va_list(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)ls->info;
	unsigned long ret_val = 0;
	
	BUG_ON(info->target != (unsigned long)&kedr_test_arg_func_va_list);
	
	if (test_failed) 
		/* If the pre handler did not set it to 0, there was an
		 * error detected there. */
		return;
	
	/* Assume a failure by default. */
	test_failed = 1;
	if (check_args_va_list(ls) != 0)
		return;
	
	ret_val = KEDR_LS_RET_VAL(ls);
	if (ret_val != (unsigned long)&kedr_test_arg_func_va_list) {
		pr_warning(KEDR_MSG_PREFIX 
		"test_arg_func_post_va_list(): the return value (0x%lx) "
		"differs from the expected one (0x%lx).\n",
			ret_val, 
			(unsigned long)&kedr_test_arg_func_va_list);
		return;
	}
	
	test_failed = 0;
}

static unsigned long target_funcs[] = {
	[0] = (unsigned long)&kedr_test_arg_func,
	[1] = (unsigned long)&kedr_test_arg_func_va,
	[2] = (unsigned long)&kedr_test_arg_func_va_list
};

static void (*pre_handlers[])(struct kedr_local_storage *) = {
	[0] = test_arg_func_pre,
	[1] = test_arg_func_pre_va,
	[2] = test_arg_func_pre_va_list
};

static void (*post_handlers[])(struct kedr_local_storage *) = {
	[0] = test_arg_func_post,
	[1] = test_arg_func_post_va,
	[2] = test_arg_func_post_va_list
};
/* ====================================================================== */

static void
fill_call_info(struct kedr_function_handlers *fh, 
	struct kedr_call_info *call_info)
{
	if (call_info->target != target_funcs[test_mode])
		/* process the requested function only */
		return;
	
	/* We do not need a replacement. */
	call_info->repl = call_info->target;
	
	/* Found appropriate handlers */
	call_info->pre_handler = pre_handlers[test_mode];
	call_info->post_handler = post_handlers[test_mode];
}

static struct kedr_function_handlers fh = {
	.owner = THIS_MODULE,
	.fill_call_info = fill_call_info,
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_set_function_handlers(NULL);
}

static int __init
test_init_module(void)
{
	int ret;
	
	BUILD_BUG_ON(ARRAY_SIZE(expected_args) != KEDR_TEST_ARGS_TOTAL);
	
	if (test_mode > 2) {
		pr_warning(KEDR_MSG_PREFIX 
			"Invalid value of 'test_mode': %u\n", test_mode);
		return -EINVAL;
	}
	
	ret = kedr_set_function_handlers(&fh);
	return ret;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
