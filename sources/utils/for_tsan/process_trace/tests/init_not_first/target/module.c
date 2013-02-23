/* A target module with extra functions in its init area. The init function
 * may be not the first one in that area.
 *
 * This module is used to check whether tsan_process_trace erroneously 
 * assumes that the init function of a target module is located at the 
 * beginning of init area. */
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

volatile unsigned long test_data = 0;

static void __exit
test_cleanup_module(void)
{
	return;
}

static noinline void __init
foo(void)
{
	pr_info("[test_init_not_first] Data: %lu.\n", test_data);
	test_data = 543;	
}

static noinline void __init
bar(void)
{
	pr_info("[test_init_not_first] Data: %lu.\n", test_data);
	test_data = 8;	
}

static noinline void __init
zarb(void)
{
	pr_info("[test_init_not_first] Data: %lu.\n", test_data);
	test_data = 1904;	
}

static int __init
test_init_module(void)
{
	foo();
	bar();
	zarb();
	--test_data;
	pr_info("[test_init_not_first] Data: %lu.\n", test_data);
	
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
