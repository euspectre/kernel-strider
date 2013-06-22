kmodule_configure_kernel_functions(functions OPTIONAL
	"__kmalloc"
	"kmem_cache_alloc"
	"__get_free_pages"
	"get_zeroed_page"
	"kmem_cache_alloc_notrace"
	"kmem_cache_alloc_trace"
	"__kmalloc_node"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmem_cache_alloc_node_trace"
	"kmalloc_order_trace"
	"__alloc_pages_nodemask"
	"alloc_pages_current"
	"alloc_pages_exact"
	"alloc_pages_exact_nid" 
	"vmalloc"
	"__vmalloc"
	"vmalloc_user"
	"vmalloc_node"
	"vmalloc_32"
	"vmalloc_32_user"
	"vzalloc"
	"vzalloc_node" 
)
group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")

kedr_test_add_subdirectory(tests)
########################################################################
