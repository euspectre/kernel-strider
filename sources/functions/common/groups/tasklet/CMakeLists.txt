set(GROUP_NAME "tasklet")

kmodule_configure_kernel_functions(functions OPTIONAL
	"__tasklet_schedule"
	"__tasklet_hi_schedule"
	"tasklet_kill"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
