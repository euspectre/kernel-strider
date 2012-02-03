set(KMODULE_TEST_NAME "test_ls_allocator")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_module(${KMODULE_TEST_NAME} 
	"module.c")

kedr_test_add_target (${KMODULE_TEST_NAME})
