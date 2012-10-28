set(GROUP_NAME "dup")

kmodule_configure_kernel_functions(functions OPTIONAL
	"kmemdup"
	"kstrdup"
	"kstrndup"

	"match_strdup"

	"memdup_user"
	"strndup_user"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
