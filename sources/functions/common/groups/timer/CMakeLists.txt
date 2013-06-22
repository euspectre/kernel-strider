kmodule_configure_kernel_functions(functions OPTIONAL
	"add_timer"
	"add_timer_on"
	"mod_timer"
	"mod_timer_pinned"

	"del_timer_sync"
	"try_to_del_timer_sync"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")

kedr_test_add_subdirectory(tests)
########################################################################
