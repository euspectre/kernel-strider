set(TOP_TEST_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(TEST_TARGET_NAME "test_arg_target")
set(TEST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/target")
set(TEST_PROVIDER_NAME "test_arg_provider")
set(TEST_PROVIDER_DIR "${CMAKE_CURRENT_BINARY_DIR}/provider")
set(TEST_CHECKER_NAME "test_arg_checker")
set(TEST_CHECKER_DIR "${CMAKE_CURRENT_BINARY_DIR}/checker")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_add_script (functions.args.01
    test.sh 0
)

kedr_test_add_script (functions.args.02
    test.sh 1
)

kedr_test_add_script (functions.args.03
    test.sh 2
)

add_subdirectory(provider)
add_subdirectory(target)
add_subdirectory(checker)
########################################################################
