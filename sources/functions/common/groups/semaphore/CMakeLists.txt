kmodule_configure_kernel_functions(functions OPTIONAL
	"down"
	"down_interruptible"
	"down_killable"
	"down_trylock"
	"down_timeout"
	"up"
)

group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")
########################################################################
