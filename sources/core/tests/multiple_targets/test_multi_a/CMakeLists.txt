set(KMODULE_TARGET_NAME "test_multi_a")

kbuild_add_module(${KMODULE_TARGET_NAME} 
	"module.c")

kedr_test_add_target (${KMODULE_TARGET_NAME})
