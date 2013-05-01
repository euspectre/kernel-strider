set(GROUP_NAME "bh")

kmodule_configure_kernel_functions(functions OPTIONAL
	"local_bh_enable"
	"local_bh_disable"
)
create_handlers_group(${GROUP_NAME} ${functions})
########################################################################
