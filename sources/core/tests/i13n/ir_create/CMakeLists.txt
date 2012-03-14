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

kedr_test_add_script (mem_core.i13n.ir_create.07
    test.sh "disp32_iprel"
)

kedr_test_add_script (mem_core.i13n.ir_create.08
    test.sh "long_blocks"
)

kedr_test_add_script (mem_core.i13n.ir_create.09
    test.sh "io_mem"
)

kedr_test_add_script (mem_core.i13n.ir_create.10
    test.sh "locked_updates"
)

kedr_test_add_script (mem_core.i13n.ir_create.11
    test.sh "common_blocks"
)

# Tests for the error conditions. The core should detect these and
# refuse to instrument the target module.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test_err.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test_err.sh"
  @ONLY
)

kedr_test_add_script (mem_core.i13n.ir_create.err.01
    test_err.sh "split_func"
)

add_subdirectory(accessor)
add_subdirectory(target_common)
add_subdirectory(target_split_func)
