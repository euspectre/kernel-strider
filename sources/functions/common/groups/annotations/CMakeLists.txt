# Handling of KEDR annotations if they are provided by the kernel itself.
kmodule_configure_kernel_functions(functions OPTIONAL
	"kedr_annotate_happens_before"
	"kedr_annotate_happens_after"
	"kedr_annotate_memory_acquired"
	"kedr_annotate_memory_released"
)

group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")	
