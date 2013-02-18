#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

/* Check that all the addresses from the kernel space have 1 in their
 * most significant bit. This bit is used by the thread handling subsystem
 * to store some additional info.
 * On some 32-bit systems with more exotic virtual memory split
 * configuration (CONFIG_VMSPLIT_2G_OPT=y or CONFIG_VMSPLIT_1G=y) this
 * may not be the case. */
#if defined(CONFIG_VMSPLIT_2G_OPT) || defined(CONFIG_VMSPLIT_1G)
# error Unsupported virtual memory split configuration.
#endif

/* 
 * The rest of the code does not really matter as long as it is correct 
 * from the compiler's point of view.
 */
static int __init
my_init(void)
{
	return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
