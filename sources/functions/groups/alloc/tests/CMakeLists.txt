kedr_load_test_prefixes()

set(functions_to_test
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

# The regular expression the trace obtained in the test should match
# for the test to pass.
set(TEST_MATCH_EXPR "ALLOC PRE.*ALLOC POST")

add_tests_for_group(${GROUP_NAME} ${functions_to_test})
########################################################################
