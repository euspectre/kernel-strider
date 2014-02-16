# This group handles netdev-related functions (alloc/free,
# register/unregister).

set(to_check
	"alloc_etherdev_mq"
	"alloc_etherdev_mqs"
	"alloc_netdev_mq"
	"alloc_netdev_mqs"

	"free_netdev"

	"register_netdev"
	"register_netdevice"
	"unregister_netdev"
	"unregister_netdevice_queue"
)

# CentOS 6, kernel 2.6.32-431-something. The vanilla 2.6.32 does not  
# provide unregister_netdevice_many() function but that kernel from CentOS
# does. However, 'unreg_list' field is still missing from struct net_device
# there. It is used by KernelStrider to handle the calls to this function.
# Let's just handle unregister_netdevice_many() only if that field exists.
# BTW, it appeared the first time in 2.6.33, along with 
# unregister_netdevice_many(). Support for 2.6.32 in KernelStrider becomes
# increasingly less important anyway...
check_struct_has_member(
	"struct net_device" 
	"unreg_list"
	"linux/netdevice.h"
	net_device_has_unreg_list)

if (net_device_has_unreg_list)
	list(APPEND to_check "unregister_netdevice_many")
endif ()

kmodule_configure_kernel_functions(functions OPTIONAL ${to_check})
group_create_handlers(${GROUP_NAME} ${functions})

group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################
