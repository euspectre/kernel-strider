#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");

static int __init
my_init(void)
{
	struct workqueue_struct *wq;
	wq = __alloc_workqueue_key("my_wq%u", 0, 1, NULL, NULL, 18);
	destroy_workqueue(wq);
	
	return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
