kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/transform")

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

kedr_test_add_script (mem_core.i13n.transform.01
	test.sh "base_reg"
)

if (NOT KEDR_64_BIT)
	# Check if the base register is chosen properly in presence
	# of the string operations. These tests are for x86-32 only,
	# because %rsi and %rdi are scratch registers on x86-64 and
	# therefore cannot be used as %base anyway.
	kedr_test_add_script (mem_core.i13n.transform.x86_32.01
		test.sh "base_reg_no_esi"
	)
	kedr_test_add_script (mem_core.i13n.transform.x86_32.02
		test.sh "base_reg_no_edi"
	)
	kedr_test_add_script (mem_core.i13n.transform.x86_32.03
		test.sh "base_reg_no_esi_edi1"
	)
	kedr_test_add_script (mem_core.i13n.transform.x86_32.04
		test.sh "base_reg_no_esi_edi2"
	)
endif ()

kedr_test_add_script (mem_core.i13n.transform.02
	test.sh "calls_jumps2_rel32"
)

kedr_test_add_script (mem_core.i13n.transform.03
	test.sh "calls_jumps2_jcc"
)

kedr_test_add_script (mem_core.i13n.transform.04
	test.sh "calls_jumps2_indirect"
)

kedr_test_add_script (mem_core.i13n.transform.05
	test.sh "calls_jumps2_jmp_short"
)

kedr_test_add_script (mem_core.i13n.transform.06
	test.sh "calls_jumps2_jcc_short"
)

kedr_test_add_script (mem_core.i13n.transform.07
	test.sh 
		"common_type_e" 
		"common_type_e_with_stack" 
		"process_stack_accesses"
)

kedr_test_add_script (mem_core.i13n.transform.08
	test.sh "io_mem"
)

kedr_test_add_script (mem_core.i13n.transform.09
	test.sh "common_type_e"
)

kedr_test_add_script (mem_core.i13n.transform.10
	test.sh "mem_special"
)

kedr_test_add_script (mem_core.i13n.transform.11
	test.sh "mem_special_xlat_bx"
)

kedr_test_add_script (mem_core.i13n.transform.12
	test.sh "strings"
)

kedr_test_add_script (mem_core.i13n.transform.13
	test.sh "locked_updates2"
)

kedr_test_add_script (mem_core.i13n.transform.14
	test.sh "barriers_mem"
)

# Tests with "NULL Allocator" (checking fallbacks, etc.)
configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test_nulla.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_nulla.sh"
	@ONLY
)

kedr_test_add_script (mem_core.i13n.null_alloc.01
	test_nulla.sh "kedr_sample_target" "${CMAKE_BINARY_DIR}/tests/sample_target"
)

kedr_test_add_script (mem_core.i13n.null_alloc.02
	test_nulla.sh "target_jtable_basic" 
		"${CMAKE_BINARY_DIR}/core/tests/i13n/target_jtable"
)

# Just in case some functions in the dummy target are instrumentable.
# Usually they are not but the calls to mcount() or the like used on some 
# systems could make these functions long enough to be instrumentable.
kedr_test_add_script (mem_core.i13n.null_alloc.03
	test_nulla.sh "kedr_dummy_target" "${CMAKE_BINARY_DIR}/tests/dummy_target"
)

# TODO: tests checking entry/exit and call pre/post events with handlers
# (on the same code as calls_jumps2_* tests?)

add_subdirectory(accessor2)
add_subdirectory(target_common)
