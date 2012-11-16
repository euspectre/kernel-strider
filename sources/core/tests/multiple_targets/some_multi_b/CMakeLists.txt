set(KMODULE_TARGET_NAME "some-multi_b")

kbuild_add_module(${KMODULE_TARGET_NAME} 
	"module.c")

kedr_test_add_target (${KMODULE_TARGET_NAME})
