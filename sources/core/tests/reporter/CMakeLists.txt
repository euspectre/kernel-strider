set(KMODULE_NAME "kedr_test_reporter")
set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr") 

# Copy the sources of the utility components
set(DEBUG_UTIL_DIR "${CMAKE_SOURCE_DIR}/core/tests/debug_util")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/debug_util.c"
	"${DEBUG_UTIL_DIR}/debug_util.c"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/debug_util.h"
	"${DEBUG_UTIL_DIR}/debug_util.h"
) 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers") 
kbuild_add_module(${KMODULE_NAME} 
# sources
	"module.c"
	"debug_util.c"

# headers
	"debug_util.h"
	
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/object_types.h"
)

kedr_test_add_target (${KMODULE_NAME})
