kedr_load_test_prefixes()

set(functions_to_test
	"kmemdup"
	"kstrdup"
	"kstrndup"
	"match_strdup"
)

# The regular expression the trace obtained in the test should match
# for the test to pass.
set(TEST_MATCH_EXPR " ALLOC PRE.* ALLOC POST")

add_tests_for_group(${PLUGIN_SHORT_NAME} ${GROUP_NAME} ${functions_to_test})
########################################################################
