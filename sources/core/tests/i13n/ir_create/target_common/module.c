/* A target module with empty init and exit functions. 
 * A number of functions will be linked into this module that are 
 * defined in the assembly sources. These functions will be used when
 * testing the creation of the IR. */
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
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
