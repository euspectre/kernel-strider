set(GROUP_NAME "timer")

kmodule_configure_kernel_functions(functions OPTIONAL
	"add_timer"
	"add_timer_on"
	"mod_timer"
	"mod_timer_pinned"

	"del_timer_sync"
	"try_to_del_timer_sync"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
