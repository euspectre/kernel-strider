kmodule_configure_kernel_functions(functions OPTIONAL
	"kmemdup"
	"kstrdup"
	"kstrndup"

	"match_strdup"

	"memdup_user"
	"strndup_user"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")

kedr_test_add_subdirectory(tests)
########################################################################
