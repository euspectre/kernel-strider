kmodule_configure_kernel_functions(functions OPTIONAL
	"krealloc"
	"__krealloc"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")

kedr_test_add_subdirectory(tests)
########################################################################
