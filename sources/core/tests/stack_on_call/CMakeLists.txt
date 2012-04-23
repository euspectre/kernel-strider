kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/stack_on_call") 
set(TEST_TARGET_NAME "test_stack_on_call")
set(TEST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/target")

if (KEDR_64_BIT)
	set(EXPECTED_DIR 
		"${CMAKE_CURRENT_SOURCE_DIR}/expected/x86-64")
else ()
	set(EXPECTED_DIR
		"${CMAKE_CURRENT_SOURCE_DIR}/expected/x86-32")
endif ()

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_add_script (mem_core.stack.01
    test.sh "stack_on_call"
)

kedr_test_add_script (mem_core.stack.02
    test.sh "stack_on_jmp"
)

add_subdirectory(target)
########################################################################
