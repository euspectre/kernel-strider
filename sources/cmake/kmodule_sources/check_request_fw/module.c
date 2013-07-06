#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");

/* Check if request_firmware_nowait() accepts 'gfp' argument. */
static void
my_cont(const struct firmware *fw, void *context)
{
	return;
}

static int __init
my_init(void)
{
	struct device device;
	int ret = request_firmware_nowait(THIS_MODULE, 0, "some_name",
					  &device, GFP_KERNEL, NULL,
					  my_cont);
	return (ret == 0);
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
