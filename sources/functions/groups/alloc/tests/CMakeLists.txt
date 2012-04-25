kedr_load_test_prefixes()

set(functions_to_test
	"__alloc_pages_nodemask"
	"__get_free_pages"
	"__kmalloc"
	"__kmalloc_node"
	"alloc_pages_current"
	"alloc_pages_exact"
	"alloc_pages_exact_nid"
	"get_zeroed_page"
	"kmalloc_order_trace"
	"kmem_cache_alloc"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmem_cache_alloc_node_trace"
	"kmem_cache_alloc_notrace"
	"kmem_cache_alloc_trace"
)

# The regular expression the trace obtained in the test should match
# for the test to pass.
set(TEST_MATCH_EXPR "ALLOC PRE.*ALLOC POST")

add_tests_for_group(${GROUP_NAME} ${functions_to_test})
########################################################################
