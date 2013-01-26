/* A dummy target module that has no code in init area, only data. */
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* This is to make sure the module has init area despite there is no code 
 * there. */
int __initdata
test_no_code_data = 0x12345678;

int
test_no_code_dummy(void)
{
	/* The function must be present but is not intended to be called. */
	pr_info("Here I am!\n");
	return 0;
}
EXPORT_SYMBOL(test_no_code_dummy);
/* ====================================================================== */
