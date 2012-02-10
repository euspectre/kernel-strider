set(KMODULE_TEST_NAME "test_module_ms_alloc")

set(KEDR_MSG_PREFIX "[kedr_test_module_ms_alloc] ")

configure_file("${CMAKE_SOURCE_DIR}/core/core_impl.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/core_impl.h"
	@ONLY
)

configure_file("${CMAKE_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/config.h"
	@ONLY
)

# Copy the sources of the subsystem under test
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/module_ms_alloc.c"
	"${CMAKE_SOURCE_DIR}/core/module_ms_alloc.c"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/module_ms_alloc.h"
	"${CMAKE_SOURCE_DIR}/core/module_ms_alloc.h"
)

kbuild_add_module(${KMODULE_TEST_NAME} 
# sources
	"module.c"
	"module_ms_alloc.c"

# headers	
	"core_impl.h"
	"module_ms_alloc.h"
)

kedr_test_add_target (${KMODULE_TEST_NAME})
