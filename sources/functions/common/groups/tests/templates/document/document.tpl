/* The target module for testing if the functions are handled properly.
 * In its init function, the module triggers the call to the target
 * function (and probaly the calls to some other function). 
 * The name of the target function is passed to the module via 
 * "target_function" parameter. 
 *
 * [NB] If you intend to use the test reporter with this module, symbol 
 * resolution should be turned off in the reporter. This is because the
 * events in the init function are to be processed. */
	
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
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/sort.h>

#include "config.h"

<$if concat(header)$><$header: join(\n)$><$endif$>
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[test_func_drd] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* The name of the function to trigger a call to. */
char *target_function = "";
module_param(target_function, charp, S_IRUGO);

/* This parameter can be used in the tests as a volatile value to prevent 
 * the compiler from over-optimizing the code. The tests should not change
 * the value of this parameter. */
unsigned int value_one = 1;
module_param(value_one, uint, S_IRUGO);
/* ====================================================================== */

/* The trigger functions. */
<$block: join(\n\n)$>
/* ====================================================================== */

struct kedr_func_trigger
{
	/* Name of the target function. */
	const char *name; 
	
	/* The trigger function. */
	void (*trigger)(void);
};

/* The map <target function's name; trigger>. */
static struct kedr_func_trigger triggers[] = {
<$if concat(function.name)$><$triggerItem: join(\n)$>
<$else$>	{NULL, NULL}<$endif$>
};
/* ====================================================================== */

static int __init
test_init_module(void)
{
	size_t i;
	struct kedr_func_trigger *found = NULL;
	
	if (target_function[0] == 0) {
		pr_warning(KEDR_MSG_PREFIX 
	"'target_function' parameter should have a non-empty value.\n");
		return -EINVAL;
	}
	
	/* [NB] As we are going to call one trigger only, it is OK for now
	 * not to optimize the lookup of the trigger. */
	for (i = 0; i < ARRAY_SIZE(triggers); ++i) {
		if (strcmp(triggers[i].name, target_function) == 0) {
			found = &triggers[i];
			break;
		}
	}
	if (found == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
			"Unknown target function: \"%s\".\n", 
			target_function);
		return -EINVAL;
	}
	
	found->trigger();
	return 0;
}

static void __exit
test_exit_module(void)
{
}

module_init(test_init_module);
module_exit(test_exit_module);
/* ====================================================================== */
