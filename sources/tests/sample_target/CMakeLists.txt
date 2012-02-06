# A sample target module for testing
set(KMODULE_NAME "kedr_sample_target")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/module.makefile.in"
	"${CMAKE_CURRENT_BINARY_DIR}/module.makefile"
	@ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	@ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/kedr_sample_target.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_sample_target"
	@ONLY
)

add_custom_target("sample_target_module"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${KMODULE_NAME}.ko"
)

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${KMODULE_NAME}.ko"
	COMMAND $(MAKE) -f module.makefile
	DEPENDS 
		"${CMAKE_CURRENT_BINARY_DIR}/cfake.c"
		"${CMAKE_CURRENT_BINARY_DIR}/cfake.h"
		"${CMAKE_CURRENT_BINARY_DIR}/Makefile"
		"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/cfake.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/cfake.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/cfake.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/cfake.h")

kedr_test_add_target("sample_target_module")
