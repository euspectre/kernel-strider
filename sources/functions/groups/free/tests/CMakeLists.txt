kedr_load_test_prefixes()

set(functions_to_test
	"__free_pages"
	"free_pages"
	"free_pages_exact"
	"kfree"
	"kmem_cache_free"
	"kzfree"
)

# The regular expression the trace obtained in the test should match
# for the test to pass.
set(TEST_MATCH_EXPR "FREE PRE.*FREE POST")

add_tests_for_group(${GROUP_NAME} ${functions_to_test})
########################################################################
