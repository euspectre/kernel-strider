set(KEDR_MEM_CORE_DEBUGFS_DIR "kedr_test_sections")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_add_script (mem_core.sections.01 
    test.sh
)

add_subdirectory (test_module)
