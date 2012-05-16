# The tests defined here check the registration of memory operations
# as well as of memory barriers.

kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/callbacks/events")

# The directory containing the expected results for the tests
if (KEDR_64_BIT)
	set (KEDR_TEST_EXPECTED_DIR 
		"${CMAKE_CURRENT_SOURCE_DIR}/expected/x86-64")
else ()
	set (KEDR_TEST_EXPECTED_DIR 
		"${CMAKE_CURRENT_SOURCE_DIR}/expected/x86-32")
endif ()

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/prepare_symbol_table.awk.in"
	"${CMAKE_CURRENT_BINARY_DIR}/prepare_symbol_table.awk"
	@ONLY
)

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)

kedr_test_add_script (mem_core.callbacks.events.01
	test.sh "common_type_e"
)

kedr_test_add_script (mem_core.callbacks.events.02
	test.sh "mem_special"
)

kedr_test_add_script (mem_core.callbacks.events.03
	test.sh "mem_special_xlat_bx"
)

kedr_test_add_script (mem_core.callbacks.events.04
	test.sh "strings"
)

kedr_test_add_script (mem_core.callbacks.events.05
	test.sh "locked_updates2"
)

kedr_test_add_script (mem_core.callbacks.events.06
	test.sh "barriers_mem"
)

# Check that the accesses to the stack are filtered out by default
kedr_test_add_script (mem_core.callbacks.stack_access.01
	test.sh "stack_access"
)

# Check that the accesses to the stack are reported if 
# 'process_stack_accesses' parameter is set to a non-zero value.
kedr_test_add_script (mem_core.callbacks.stack_access.02
	test.sh "stack_access" --with-stack
)
