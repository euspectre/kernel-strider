set(GROUP_NAME "free")

kmodule_configure_kernel_functions(functions OPTIONAL
	"kfree"
	"kzfree"
	# TODO: more
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
