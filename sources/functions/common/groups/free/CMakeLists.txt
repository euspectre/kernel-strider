kmodule_configure_kernel_functions(functions OPTIONAL
	"kfree"
	"kzfree"
	"kmem_cache_free"
	"free_pages"
	"__free_pages"
	"free_pages_exact" 
	"vfree"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")

kedr_test_add_subdirectory(tests)
########################################################################
