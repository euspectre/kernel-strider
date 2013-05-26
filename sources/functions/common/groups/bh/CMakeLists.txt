kmodule_configure_kernel_functions(functions OPTIONAL
	"local_bh_enable"
	"local_bh_disable"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_set_sources(${PLUGIN_SHORT_NAME} ${GROUP_NAME} 
	"handlers_${GROUP_NAME}.c"
)
########################################################################
