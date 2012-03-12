kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/ir_create")

# The directory containing the expected results for the tests
if (KEDR_64_BIT)
	set (KEDR_TEST_EXPECTED_DIR 
		"${CMAKE_CURRENT_SOURCE_DIR}/expected/x86-64")
else ()
	set (KEDR_TEST_EXPECTED_DIR 
		"${CMAKE_CURRENT_SOURCE_DIR}/expected/x86-32")
endif ()

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_add_script (mem_core.i13n.ir_create.01
    test.sh "barriers_cpuid"
)

kedr_test_add_script (mem_core.i13n.ir_create.02
    test.sh "jump_tables"
)

kedr_test_add_script (mem_core.i13n.ir_create.03
    test.sh "calls_jumps"
)

kedr_test_add_script (mem_core.i13n.ir_create.04
    test.sh "jcxz_loop"
)

kedr_test_add_script (mem_core.i13n.ir_create.05
    test.sh "special_control"
)

kedr_test_add_script (mem_core.i13n.ir_create.06
    test.sh "barriers_other"
)
# TODO: more tests

add_subdirectory(accessor)
add_subdirectory(target_common)
# TODO: target to test the core on "split" functions, etc.
