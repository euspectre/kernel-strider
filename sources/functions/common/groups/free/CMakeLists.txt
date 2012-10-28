set(GROUP_NAME "free")

kmodule_configure_kernel_functions(functions OPTIONAL
	"kfree"
	"kzfree"
	"kmem_cache_free"
	"free_pages"
	"__free_pages"
	"free_pages_exact" 
	"vfree"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
