configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test_basics.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test_basics.sh"
  @ONLY
)

kedr_test_add_script (mem_core.ls_allocator.01 
    test_basics.sh
)

add_subdirectory (test_module)
