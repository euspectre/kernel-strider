configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test_unload.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test_unload.sh"
  @ONLY
)

kedr_test_add_script (mem_core.providers.01
    test_unload.sh "test_eh"
)

kedr_test_add_script (mem_core.providers.02
    test_unload.sh "test_ls_alloc"
)

kedr_test_add_script (mem_core.providers.03
    test_unload.sh "test_core_hooks"
)

add_subdirectory (test_eh)
add_subdirectory (test_ls_alloc)
add_subdirectory (test_core_hooks)
