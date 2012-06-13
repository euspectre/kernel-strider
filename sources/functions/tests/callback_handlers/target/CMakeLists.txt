set(KMODULE_NAME ${TEST_TARGET_NAME})

kbuild_use_symbols("${TEST_CB_USER_DIR}/Module.symvers") 
kbuild_add_dependencies("${TEST_CB_USER_NAME}")
kbuild_include_directories("${TOP_TEST_INCLUDE_DIR}")
kbuild_add_module(${KMODULE_NAME} 
# sources
	"module.c"
# headers
	"${TOP_TEST_INCLUDE_DIR}/test_cbh.h"
)
kedr_test_add_target(${KMODULE_NAME})
########################################################################
