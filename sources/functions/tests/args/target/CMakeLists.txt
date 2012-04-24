set(KMODULE_NAME "test_arg_target")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/functions/tests/args/provider/Module.symvers") 
kbuild_add_dependencies("test_arg_provider")
kbuild_include_directories("${TOP_TEST_INCLUDE_DIR}")
kbuild_add_module(${KMODULE_NAME} 
# sources
	"module.c"
# headers
	"${TOP_TEST_INCLUDE_DIR}/test_arg.h"
)
kedr_test_add_target(${KMODULE_NAME})
########################################################################
