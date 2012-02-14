/* A module to test if the provider of the event handlers is unloadable
 * while the target is in memory. */

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
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <kedr/kedr_mem/core_api.h> 

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

static void 
test_on_target_loaded(struct kedr_event_handlers *eh, 
	struct module *target_module)
{
}

static void 
test_on_target_about_to_unload(struct kedr_event_handlers *eh, 
	struct module *target_module)
{
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
