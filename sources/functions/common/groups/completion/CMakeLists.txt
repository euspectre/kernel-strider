kmodule_configure_kernel_functions(functions OPTIONAL
	"wait_for_completion"
	"wait_for_completion_timeout"
	"wait_for_completion_io"
	"wait_for_completion_io_timeout"
	"wait_for_completion_interruptible"
	"wait_for_completion_interruptible_timeout"
	"wait_for_completion_killable"
	"wait_for_completion_killable_timeout"
	"try_wait_for_completion"
	
	"complete"
	"complete_all"
)
group_create_handlers(${GROUP_NAME} ${functions})

group_add_sources("handlers_${GROUP_NAME}.c")

kedr_test_add_subdirectory(tests)
########################################################################
