set(KMODULE_NAME ${TEST_TARGET_NAME})
kbuild_add_module(${KMODULE_NAME} 
	"module.c"
)
kedr_test_add_target(${KMODULE_NAME})
########################################################################
