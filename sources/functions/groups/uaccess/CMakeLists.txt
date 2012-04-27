set(GROUP_NAME "uaccess")

kmodule_configure_kernel_functions(functions OPTIONAL
	"copy_to_user"
	"_copy_to_user"
	"copy_from_user"
	"_copy_from_user"
	"__strncpy_from_user"
	"strncpy_from_user"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
