set(KMODULE_TEST_NAME "test_accessor2")

set(KEDR_MSG_PREFIX "[${KMODULE_TEST_NAME}] ")

configure_file("${CMAKE_SOURCE_DIR}/core/core_impl.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/core_impl.h"
	@ONLY
)

configure_file("${CMAKE_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/config.h"
	@ONLY
)

# Copy the internal headers of the core to the current build directory,
# for convenience.
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/i13n.h"
	"${CMAKE_SOURCE_DIR}/core/i13n.h"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/ir.h"
	"${CMAKE_SOURCE_DIR}/core/ir.h"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/ifunc.h"
	"${CMAKE_SOURCE_DIR}/core/ifunc.h"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/hooks.h"
	"${CMAKE_SOURCE_DIR}/core/hooks.h"
)

# Copy the sources of the utility components
set(DEBUG_UTIL_DIR "${CMAKE_SOURCE_DIR}/core/tests/debug_util")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/debug_util.c"
	"${DEBUG_UTIL_DIR}/debug_util.c"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/debug_util.h"
	"${DEBUG_UTIL_DIR}/debug_util.h"
) 

set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr") 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers") 
kbuild_add_module(${KMODULE_TEST_NAME} 
# sources
	"module.c"
	"debug_util.c"

# headers	
	"config.h"
	"core_impl.h"
	"debug_util.h"
	"i13n.h"
	"ir.h"
	"ifunc.h"
	"hooks.h"

	"${TOP_INCLUDE_DIR}/kedr_mem/block_info.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/functions.h"
)

kedr_test_add_target (${KMODULE_TEST_NAME})
