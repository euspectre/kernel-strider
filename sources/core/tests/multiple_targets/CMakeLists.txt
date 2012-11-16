kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/multiple_targets")

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test_basics.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_basics.sh"
	@ONLY
)

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test_init_exit.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_init_exit.sh"
	@ONLY
)

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test_broken.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_broken.sh"
	@ONLY
)

kedr_test_add_script (mem_core.multi.basics.01
	test_basics.sh
)

kedr_test_add_script (mem_core.multi.init_exit.01
	test_init_exit.sh
)

kedr_test_add_script (mem_core.multi.broken.01
	test_broken.sh
)

add_subdirectory(kedr_multi_a)
add_subdirectory(test_multi_a)
add_subdirectory(some_multi_a)
add_subdirectory(some_multi_b)
########################################################################
