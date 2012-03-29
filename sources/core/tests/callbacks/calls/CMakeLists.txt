# The tests defined here check the registration of function entry/exit
# events as well as call pre/post events.

kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/callbacks/calls")

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

kedr_test_add_script (mem_core.callbacks.calls.01
	test.sh "calls_jumps2_rel32"
)

kedr_test_add_script (mem_core.callbacks.calls.02
	test.sh "calls_jumps2_jcc"
)

kedr_test_add_script (mem_core.callbacks.calls.03
	test.sh "calls_jumps2_indirect"
)

kedr_test_add_script (mem_core.callbacks.calls.04
	test.sh "calls_jumps2_jmp_short"
)

kedr_test_add_script (mem_core.callbacks.calls.05
	test.sh "calls_jumps2_jcc_short"
)
