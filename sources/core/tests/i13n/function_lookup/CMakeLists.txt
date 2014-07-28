kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/function_lookup")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/check_data.awk.in"
  "${CMAKE_CURRENT_BINARY_DIR}/check_data.awk"
  @ONLY
)

# TODO: update the tests so that they do not rely on the addresses of 
# ELF sections, then enable them again
#kedr_test_add_script (mem_core.i13n.func_lookup.01 
#    test.sh "sample_target"
#)
#kedr_test_add_script (mem_core.i13n.func_lookup.02 
#    test.sh "dummy_target"
#)

add_subdirectory (test_module)
