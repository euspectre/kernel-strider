/* A module to test support for load/unload events. */

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

#include <kedr/kedr_mem/core_api.h> 

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* The parameters indicating whether the callbacks have been called. 
 * 1. Initially, both should be 0. 
 * 2. After the callback for "on_load" event is called, 'called_load_cb' 
 *    should be 1, 'called_unload_cb' should be 0.
 * 3. After the callback for "on_about_to_unload" event is called, 
 *    'called_load_cb' should be 0, 'called_unload_cb' should be 1. */
int called_load_cb = 0;
module_param(called_load_cb, int, S_IRUGO);

int called_unload_cb = 0;
module_param(called_unload_cb, int, S_IRUGO);

/* This module also checks the arguments passed to the callbacks. If the 
 * arguments are incorrect, 'arg_check_failed' will be 1. */
int arg_check_failed = 0;
module_param(arg_check_failed, int, S_IRUGO);

/* The expected name of the target module. */
static const char *target_name = "test_target";

/* ====================================================================== */

struct kedr_event_handlers *peh = NULL;

static void 
test_on_target_loaded(struct kedr_event_handlers *eh, 
	struct module *target_module)
{
	const char *name;
	
	called_load_cb = 1;
	called_unload_cb = 0;
	
	if (eh != peh) {
		arg_check_failed = 1;
		pr_warning("[kedr_test] "
		"\"on_load\": got eh=%p but %p was expected\n", 
			eh, peh);
		return;
	}
	
	if (target_module == NULL) {
		arg_check_failed = 1;
		pr_warning("[kedr_test] "
		"\"on_load\": 'target_module' is NULL\n");
		return;
	}
	
	name = module_name(target_module);
	if (strcmp(name, target_name) != 0) {
		arg_check_failed = 1;
		pr_warning("[kedr_test] "
	"\"on_load\": target name is \"%s\" but \"%s\" was expected\n",
			name, target_name);
		return;
	}
}

static void 
test_on_target_about_to_unload(struct kedr_event_handlers *eh, 
	struct module *target_module)
{
	const char *name;
	
	called_load_cb = 0;
	called_unload_cb = 1;
	
	if (eh != peh) {
		arg_check_failed = 1;
		pr_warning("[kedr_test] "
		"\"on_about_to_unload\": got eh=%p but %p was expected\n", 
			eh, peh);
		return;
	}
	
	if (target_module == NULL) {
		arg_check_failed = 1;
		pr_warning("[kedr_test] "
		"\"on_about_to_unload\": 'target_module' is NULL\n");
		return;
	}
	
	name = module_name(target_module);
	if (strcmp(name, target_name) != 0) {
		arg_check_failed = 1;
		pr_warning("[kedr_test] "
		"\"on_about_to_unload\": "
		"target name is \"%s\" but \"%s\" was expected\n",
			name, target_name);
		return;
	}
}

struct kedr_event_handlers test_eh = {
	.owner = THIS_MODULE,
	.on_target_loaded = test_on_target_loaded,
	.on_target_about_to_unload = test_on_target_about_to_unload,
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_unregister_event_handlers(&test_eh);
	return;
}

static int __init
test_init_module(void)
{
	int ret = 0;
	peh = &test_eh;

	ret = kedr_register_event_handlers(&test_eh);
	if (ret != 0) {
		pr_warning("[kedr_test] "
		"kedr_register_event_handlers() failed, error code: %d\n", 
			ret);
		return ret;
	}
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
