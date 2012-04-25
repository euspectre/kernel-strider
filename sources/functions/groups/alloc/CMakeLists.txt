set(GROUP_NAME "alloc")

kmodule_configure_kernel_functions(functions OPTIONAL
	"__kmalloc"
	"__get_free_pages"
	# TODO: more
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
