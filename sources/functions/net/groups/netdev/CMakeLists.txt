# This group handles netdev-related functions (alloc/free,
# register/unregister).

kmodule_configure_kernel_functions(functions OPTIONAL
	"alloc_etherdev_mq"
	"alloc_etherdev_mqs"
	"alloc_netdev_mq"
	"alloc_netdev_mqs"

	"free_netdev"

	"register_netdev"
	"register_netdevice"
	"unregister_netdev"
	"unregister_netdevice_many"
	"unregister_netdevice_queue"
)
group_create_handlers(${GROUP_NAME} ${functions})

group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################
