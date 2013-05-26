kmodule_configure_kernel_functions(functions OPTIONAL
	"copy_to_user"
	"_copy_to_user"
	"copy_from_user"
	"_copy_from_user"
	"__strncpy_from_user"
	"strncpy_from_user"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_set_sources(${PLUGIN_SHORT_NAME} ${GROUP_NAME} 
	"handlers_${GROUP_NAME}.c"
)

kedr_test_add_subdirectory(tests)
########################################################################
