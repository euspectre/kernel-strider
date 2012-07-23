# Check that the kernel module from this example can be built, 
# loaded, etc. 
kedr_load_test_prefixes()
set(KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/examples/buggy01")

add_custom_target("test_buggy01"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${KMODULE_NAME}.ko"
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile_installed.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_build.mk"
	@ONLY
)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../Kbuild.in"
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	@ONLY
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${KMODULE_NAME}.ko"
	COMMAND $(MAKE) -f test_build.mk
	DEPENDS 
		"${CMAKE_CURRENT_BINARY_DIR}/module.c"
		"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
		"${CMAKE_CURRENT_BINARY_DIR}/test_build.mk"
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/module.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../module.c")

kedr_test_add_target("test_buggy01")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)

kedr_test_add_script(examples.buggy.01
	test.sh
)

