#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>

MODULE_LICENSE("GPL");

#if defined(IS_NDO_FDB_ADD_DEV2)
static int 
my_func(struct ndmsg *ndm, struct net_device *dev, unsigned char *addr,
	u16 flags)
{
	return 0;
}
#elif defined(IS_NDO_FDB_ADD_DEV3)
static int 
my_func(struct ndmsg *ndm, struct nlattr *tb[], struct net_device *dev, 
	const unsigned char *addr, u16 flags)
{
	return 0;
}	
#else
# error "Unknown request"
#endif

struct net_device dev;
struct net_device_ops ops;

static int __init
my_init(void)
{
	ops.ndo_fdb_add = my_func;
	dev.netdev_ops = &ops;	
	
	return register_netdev(&dev);
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
