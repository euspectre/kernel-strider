kmodule_configure_kernel_functions(functions REQUIRED
	ONE_OF_BEGIN "kthread_create" "kthread_create_on_node" ONE_OF_END
	"kthread_stop"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")
########################################################################
