set(TOP_TEST_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(TEST_TARGET_NAME "test_cbh_target")
set(TEST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/target")
set(TEST_CB_USER_NAME "test_cbh_user")
set(TEST_CB_USER_DIR "${CMAKE_CURRENT_BINARY_DIR}/cb_user")
set(TEST_CHECKER_NAME "test_cbh_checker")
set(TEST_CHECKER_DIR "${CMAKE_CURRENT_BINARY_DIR}/checker")

kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/callback_handlers")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_add_script (functions.callback_handlers.01
    test.sh
)

kedr_test_add_script (functions.callback_handlers.02
    test.sh "no_lookup"
)

add_subdirectory(cb_user)
add_subdirectory(target)
add_subdirectory(checker)
########################################################################
