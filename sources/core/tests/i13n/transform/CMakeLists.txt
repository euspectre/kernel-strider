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

# TODO Uncomment when writing tests begins
#configure_file (
#  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
#  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
#  @ONLY
#)
#
#kedr_test_add_script (mem_core.i13n.transform.01
#    test.sh "???????"
#)

add_subdirectory(accessor2)
#add_subdirectory(target_common)
