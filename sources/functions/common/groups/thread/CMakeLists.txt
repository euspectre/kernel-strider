set(GROUP_NAME "thread")

kmodule_configure_kernel_functions(functions REQUIRED
	ONE_OF_BEGIN "kthread_create" "kthread_create_on_node" ONE_OF_END
	"kthread_stop"
)
create_handlers_group(${GROUP_NAME} ${functions})
########################################################################
