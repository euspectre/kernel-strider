set(KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/bug_bench")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/test_sample_and_buggy01.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_sample_and_buggy01.sh"
	@ONLY
)

kedr_test_add_script(bug_bench.01
	test_sample_and_buggy01.sh
)

add_subdirectory(test_sample_target)
########################################################################
