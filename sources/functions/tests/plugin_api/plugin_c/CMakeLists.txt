set(KMODULE_NAME "test_fh_plugin_c")

kbuild_use_symbols(
	"${CMAKE_BINARY_DIR}/core/Module.symvers"
) 
kbuild_add_dependencies("kedr_mem_core")

kbuild_add_module(${KMODULE_NAME} 
# sources
	"module.c"

# headers
	"${TOP_INCLUDE_DIR}/kedr_mem/functions.h"
)
kedr_test_add_target(${KMODULE_NAME})
########################################################################
