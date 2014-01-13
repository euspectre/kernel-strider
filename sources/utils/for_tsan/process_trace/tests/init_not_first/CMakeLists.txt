set(KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/init_not_first")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)

kedr_test_add_script(utils.init_not_first.01
	test.sh
)

add_subdirectory(target)
########################################################################
