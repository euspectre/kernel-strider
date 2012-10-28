set(GROUP_NAME "realloc")

kmodule_configure_kernel_functions(functions OPTIONAL
	"krealloc"
	"__krealloc"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
