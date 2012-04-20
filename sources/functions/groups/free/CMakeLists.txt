kmodule_configure_kernel_functions(functions OPTIONAL
	"kfree"
	"kzfree"
	# TODO: more
)
create_handlers_group("free" ${functions})
########################################################################
