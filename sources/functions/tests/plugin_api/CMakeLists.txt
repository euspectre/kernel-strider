set(TOP_TEST_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(TEST_TARGET_NAME "test_fh_api_target")
set(TEST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/target")

kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/fh_api")

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)

kedr_test_add_script (functions.fh_api.01
	test.sh
)

add_subdirectory(target)
add_subdirectory(plugin_a)
add_subdirectory(plugin_b)
add_subdirectory(plugin_c)
########################################################################
