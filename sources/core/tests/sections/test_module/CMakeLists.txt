set(KMODULE_TEST_NAME "test_sections")

set(KEDR_MSG_PREFIX "[kedr_test_sections] ")
set(KEDR_UM_HELPER_PATH "${CMAKE_CURRENT_BINARY_DIR}")

configure_file("${CMAKE_SOURCE_DIR}/core/core_impl.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/core_impl.h"
	@ONLY
)

configure_file("${CMAKE_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/config.h"
	@ONLY
)

configure_file("${CMAKE_SOURCE_DIR}/core/core_impl.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/core_impl.h"
	@ONLY
)

configure_file("${CMAKE_SOURCE_DIR}/core/um_helpers/kedr_get_sections.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_get_sections.sh"
	@ONLY
)

# Copy the sources of the subsystem under test
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/sections.c"
	"${CMAKE_SOURCE_DIR}/core/sections.c"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/sections.h"
	"${CMAKE_SOURCE_DIR}/core/sections.h"
)

# Copy the sources of the utility components
set(DEBUG_UTIL_DIR "${CMAKE_SOURCE_DIR}/core/tests/debug_util")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/debug_util.c"
	"${DEBUG_UTIL_DIR}/debug_util.c"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/debug_util.h"
	"${DEBUG_UTIL_DIR}/debug_util.h"
)

kbuild_add_module(${KMODULE_TEST_NAME} 
# sources
	"module.c"
	"sections.c"
	"debug_util.c"

# headers	
	"core_impl.h"
	"sections.h"
	"debug_util.h"
)

kedr_test_add_target (${KMODULE_TEST_NAME})
