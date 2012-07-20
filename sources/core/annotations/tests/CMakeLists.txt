kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/annotations")
set(KMODULE_TEST_NAME "test_annotations")

# A local version of the user-mode helper script. It is needed
# because the tests are expected to be run before installation
# of our system. It is more reliable to have the script locally.
configure_file(
	"${CMAKE_SOURCE_DIR}/core/um_helpers/kedr_get_sections.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_get_sections.sh"
	@ONLY
)
# The directory where the tests should look for the script.
set(KEDR_UMH_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# The regular expression the reporter's output should match for the test 
# to pass
if (KEDR_64_BIT)
	set (TEST_MATCH_LINES 
		"SIGNAL COMMON PRE id=f1234567baadf00d"
		"SIGNAL COMMON POST id=f1234567baadf00d"
		"WAIT COMMON PRE id=f1234567baadf00d"
		"WAIT COMMON POST id=f1234567baadf00d"
		"ALLOC PRE size=18"
		"ALLOC POST addr=aedf1234deadbeef size=18"
		"FREE PRE addr=aedf1234deadbeef"
		"FREE POST addr=aedf1234deadbeef")
else ()
	set (TEST_MATCH_LINES 
		"SIGNAL COMMON PRE id=baadf00d"
		"SIGNAL COMMON POST id=baadf00d"
		"WAIT COMMON PRE id=baadf00d"
		"WAIT COMMON POST id=baadf00d"
		"ALLOC PRE size=18"
		"ALLOC POST addr=deadbeef size=18"
		"FREE PRE addr=deadbeef"
		"FREE POST addr=deadbeef")
endif ()

set(TEST_MATCH_EXPR)
foreach (match_line ${TEST_MATCH_LINES})
	set(TEST_MATCH_EXPR "${TEST_MATCH_EXPR}${match_line}")
endforeach ()

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)
kedr_test_add_script (mem_core.annotations.01
	test.sh
)

add_subdirectory (test_module) 
