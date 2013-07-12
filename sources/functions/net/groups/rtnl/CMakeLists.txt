# This group handles rtnl_lock(), etc.

kmodule_configure_kernel_functions(functions OPTIONAL
	"rtnl_lock"
	"rtnl_unlock"
	"rtnl_trylock"
)
group_create_handlers(${GROUP_NAME} ${functions})

group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################
