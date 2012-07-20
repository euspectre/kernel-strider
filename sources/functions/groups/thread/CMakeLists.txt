set(GROUP_NAME "thread")

kmodule_configure_kernel_functions(functions REQUIRED
	"kthread_create"
	"kthread_stop"
)
create_handlers_group(${GROUP_NAME} ${functions})
########################################################################
