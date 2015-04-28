#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>

MODULE_LICENSE("GPL");

#if defined(IS_NDO_FDB_DEL_DEV2)
static int 
my_func(struct ndmsg *ndm, struct net_device *dev, 
	const unsigned char *addr)
{
	return 0;
}
#elif defined(IS_NDO_FDB_DEL_DEV2_NOCONST)
static int 
my_func(struct ndmsg *ndm, struct net_device *dev, 
	unsigned char *addr)
{
	return 0;
}
#elif defined(IS_NDO_FDB_DEL_DEV3)
/* What is actually needed here, its to check if 'struct net_device *' is 
 * the third argument to ndo_fdb_del. */
#  if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
static int 
my_func(struct ndmsg *ndm, struct nlattr *tb[], struct net_device *dev, 
	const unsigned char *addr, u16 vid)
{
	return 0;
}
#  else
static int 
my_func(struct ndmsg *ndm, struct nlattr *tb[], struct net_device *dev, 
	const unsigned char *addr)
{
	return 0;
}
#  endif
#else
# error "Unknown request"
#endif

struct net_device dev;
struct net_device_ops ops;

static int __init
my_init(void)
{
	ops.ndo_fdb_del = my_func;
	dev.netdev_ops = &ops;	
	
	return register_netdev(&dev);
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
