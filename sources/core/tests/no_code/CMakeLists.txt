# Check that the modules with the init area that contains no
# code (only) data, are handled correctly. crc*.ko are the real examples
# of such modules.
# Before the handling of such modules was fixed in the core, the core 
# attempted to allocate memory for the fallback instances of the functions
# in the init area although there were none there.
# So, this is a regression test among other things.

kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/no_code")

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)

kedr_test_add_script (mem_core.no_code.01
	test.sh
)

add_subdirectory(test_no_code)
########################################################################
