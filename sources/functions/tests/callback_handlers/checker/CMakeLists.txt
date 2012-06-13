set(KMODULE_NAME ${TEST_CHECKER_NAME})

kbuild_use_symbols(
	"${CMAKE_BINARY_DIR}/core/Module.symvers"
	"${TEST_CB_USER_DIR}/Module.symvers"
) 
kbuild_add_dependencies("kedr_mem_core" ${TEST_CB_USER_NAME})
kbuild_include_directories("${TOP_TEST_INCLUDE_DIR}")
kbuild_add_module(${KMODULE_NAME} 
# sources
	"module.c"
		
# headers
	"${TOP_TEST_INCLUDE_DIR}/test_cbh.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/functions.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/local_storage.h"
)
kedr_test_add_target(${KMODULE_NAME})
########################################################################
