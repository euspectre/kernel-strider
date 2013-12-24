# Check that the kernel module from this example can be built, 
# loaded, etc. 
kedr_load_test_prefixes()
set(KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/examples/${MAIN_NAME}")

add_custom_target("test_${MAIN_NAME}"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${KMODULE_NAME}.ko"
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile_installed.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_build.mk"
	@ONLY
)
configure_file("${KBUILD_IN_FILE}"
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	@ONLY
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_annotations.h"
	"${CMAKE_SOURCE_DIR}/core/annotations/kedr_annotations.h"
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_annotations.c"
	"${CMAKE_SOURCE_DIR}/core/annotations/kedr_annotations.c"
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${KMODULE_NAME}.ko"
	COMMAND $(MAKE) -f test_build.mk
	DEPENDS 
		"${CMAKE_CURRENT_BINARY_DIR}/module.c"
		"${CMAKE_CURRENT_BINARY_DIR}/kedr_annotations.c"
		"${CMAKE_CURRENT_BINARY_DIR}/kedr_annotations.h"
		"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
		"${CMAKE_CURRENT_BINARY_DIR}/test_build.mk"
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/module.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../module.c")

kedr_test_add_target("test_${MAIN_NAME}")

configure_file(
	"${CMAKE_SOURCE_DIR}/examples/buggy01/tests/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)

kedr_test_add_script(examples.buggy.02
	test.sh
)

