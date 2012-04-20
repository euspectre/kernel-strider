kmodule_configure_kernel_functions(functions OPTIONAL
	"__kmalloc"
	"__get_free_pages"
	# TODO: more
)
create_handlers_group("alloc" ${functions})
########################################################################
