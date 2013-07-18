# This group handles rtnl_lock(), etc.

kmodule_configure_kernel_functions(functions OPTIONAL
	"rtnl_lock"
	"rtnl_unlock"
	"rtnl_trylock"

	"__rtnl_link_register"
	"rtnl_link_register"
	"__rtnl_link_unregister"
	"rtnl_link_unregister"
)
group_create_handlers(${GROUP_NAME} ${functions})

group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################

add_subdirectory(rtnl_link_ops)
############################################################################
