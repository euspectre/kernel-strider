kedr_load_test_prefixes()
set(TEST_MATCH_EXPR " NOT_USED")

# From the build system's point of view, it is convenient to call
# add_tests_for_group() anyway, even if there are no functions to test.
add_tests_for_group(${GROUP_NAME} "NO_FUNCTIONS_TO_TEST")
########################################################################
