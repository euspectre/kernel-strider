kmodule_configure_kernel_functions(functions OPTIONAL
	"local_bh_enable"
	"local_bh_disable"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")
########################################################################
