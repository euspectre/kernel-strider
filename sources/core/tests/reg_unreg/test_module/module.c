/* A module to test registration/de-registration of event handlers. */

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
#include <linux/string.h>
#include <linux/kernel.h>

#include <kedr/kedr_mem/core_api.h> 

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* "scenario" parameter - number of the test scenario to use:
 * 0 - reg(eh1), unreg(eh1), reg(eh1), unreg(eh1), reg(eh2), unreg(eh2). 
 *     These actions should complete without errors.
 * 1 - reg(eh1), reg(eh1), unreg(eh1). The second reg() call should fail. 
 * 2 - reg(eh1), reg(eh2), unreg(eh1). The second reg() call should fail. */
int scenario = 0;
module_param(scenario, int, S_IRUGO);

/* "test_failed" - test result, 0 - passed, any other value - failed */
int test_failed = 0; 
module_param(test_failed, int, S_IRUGO);
/* ====================================================================== */

struct kedr_event_handlers eh1;
struct kedr_event_handlers eh2;

static void
test_normal_case(void)
{
	int ret = 0;
	test_failed = 1;
	
	ret = kedr_register_event_handlers(&eh1);
	if (ret != 0) {
		pr_warning("[kedr_test] The first call to "
			"kedr_register_event_handlers(&eh1) returned %d\n", 
			ret);
		return;
	}
	kedr_unregister_event_handlers(&eh1);
	
	/* Do it again, just in case */
	ret = kedr_register_event_handlers(&eh1);
	if (ret != 0) {
		pr_warning("[kedr_test] The second call to "
			"kedr_register_event_handlers(&eh1) returned %d\n", 
			ret);
		return;
	}
	kedr_unregister_event_handlers(&eh1);
	
	/* Do it again but with another set of handlers */
	ret = kedr_register_event_handlers(&eh2);
	if (ret != 0) {
		pr_warning("[kedr_test] "
			"kedr_register_event_handlers(&eh2) returned %d\n", 
			ret);
		return;
	}
	kedr_unregister_event_handlers(&eh2);
	
	test_failed = 0; /* test passed */
}

static void
test_double_registration(struct kedr_event_handlers *eh)
{
	int ret = 0;
	test_failed = 1;
	
	ret = kedr_register_event_handlers(&eh1);
	if (ret != 0) {
		pr_warning("[kedr_test] The first call to "
			"kedr_register_event_handlers(&eh1) returned %d\n", 
			ret);
		return;
	}
	
	ret = kedr_register_event_handlers(eh);
	if (ret != -EINVAL) {
		pr_warning("[kedr_test] The call to "
			"kedr_register_event_handlers(eh) returned %d," 
			"but it was expected to return -EINVAL.\n", 
			ret);
		return;
	}
	kedr_unregister_event_handlers(&eh1);
	
	test_failed = 0; /* test passed */
}

static int
do_test(void)
{
	switch (scenario) {
	case 0:
		test_normal_case();
		break;
	case 1:
		/* trying to register the same set of handlers twice */
		test_double_registration(&eh1);
		break;
	case 2:
		/* trying to register 'eh2' while 'eh1' is registered */
		test_double_registration(&eh2);
		break;
	default: 
		/* Unknown test scenario */
		return -EINVAL;
	};
	return 0;
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
	int ret = 0;

	memset(&eh1, 0, sizeof(struct kedr_event_handlers));
	eh1.owner = THIS_MODULE;
	memset(&eh2, 0, sizeof(struct kedr_event_handlers));
	eh2.owner = THIS_MODULE;
	
	ret = do_test();

	/* Whether the test passes of fails, initialization of the module 
	* should succeed except if an invalid value has been passed for 
	* "scenario" parameter. */
	return ret;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
