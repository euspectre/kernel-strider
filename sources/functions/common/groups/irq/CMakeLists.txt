set(GROUP_NAME "irq")

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
create_handlers_group(${GROUP_NAME} ${functions})
########################################################################
