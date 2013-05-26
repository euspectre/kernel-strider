kmodule_configure_kernel_functions(functions OPTIONAL
	"request_threaded_irq"
	"request_any_context_irq"
	"free_irq"
	"enable_irq"
	"disable_irq"
	"synchronize_irq"
	"devm_request_threaded_irq"
	"devm_free_irq"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_set_sources(${PLUGIN_SHORT_NAME} ${GROUP_NAME} 
	"handlers_${GROUP_NAME}.c"
)
########################################################################
