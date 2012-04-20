kmodule_configure_kernel_functions(functions OPTIONAL
	"krealloc"
	"__krealloc"
)
create_handlers_group("realloc" ${functions})
########################################################################
