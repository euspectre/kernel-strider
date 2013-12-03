/* A module with recursive calls. */
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

unsigned int arg = 2;
module_param(arg, uint, S_IRUGO);
/* ====================================================================== */

static int fib(int n)
{
	/* This is to make sure that something is put on stack in this
	 * function. */
	unsigned int val[16];
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(val); ++i)
		val[i] = 0xbeeff00d + arg;

	BUG_ON(n > ARRAY_SIZE(val));

	if (n == 0 && val[n] > 0xbeef0000) {
		return 0;
	}
	else if (n == 1 && val[n] > 0xbeef0005) {
		return 1;
	}
	else if (val[n] > 0xbeef0000 + (unsigned int)n) {
		return fib(n - 2) + fib(n - 1);
	}

	BUG();
	return -1; /* In case BUG() stuff is not enabled. */
}

static void __exit
test_cleanup_module(void)
{
	pr_info("[test_recursion] %d (testing, ignore this message).\n",
		fib(arg + 1));
	return;
}

static int __init
test_init_module(void)
{
	pr_info("[test_recursion] %d (testing, ignore this message).\n",
		fib(arg));
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
