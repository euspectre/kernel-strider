kedr_load_test_prefixes()

set(TEST_MATCH_EXPR "")
set(TEST_CHECKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/check_trace.sh")

add_tests_for_group(${PLUGIN_SHORT_NAME} ${GROUP_NAME} ${functions})
########################################################################
