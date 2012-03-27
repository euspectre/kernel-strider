set(KMODULE_TARGET_NAME "test_ir_transform")

kbuild_add_module(${KMODULE_TARGET_NAME} 
	"module.c"

# Assembly sources with the functions to be used for testing
	"${KEDR_TEST_ASM_DIR}/base_reg.S"
	"${KEDR_TEST_ASM_DIR}/calls_jumps2.S"
)

kedr_test_add_target (${KMODULE_TARGET_NAME})
