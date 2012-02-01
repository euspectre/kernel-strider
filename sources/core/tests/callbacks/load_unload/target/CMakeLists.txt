set(KMODULE_NAME "test_target")

kbuild_add_module(${KMODULE_NAME} 
	"module.c")

kedr_test_add_target (${KMODULE_NAME})
