set(KMODULE_TARGET_NAME "test_ir_create_common")

kbuild_add_module(${KMODULE_TARGET_NAME} 
	"module.c"

# Assembly sources with the functions to be used for testing
	"${KEDR_TEST_ASM_DIR}/barriers_cpuid.S"
	"${KEDR_TEST_ASM_DIR}/jump_tables.S"
	"${KEDR_TEST_ASM_DIR}/calls_jumps.S"
	"${KEDR_TEST_ASM_DIR}/jcxz_loop.S"
	"${KEDR_TEST_ASM_DIR}/special_control.S"
	"${KEDR_TEST_ASM_DIR}/barriers_other.S"
	"${KEDR_TEST_ASM_DIR}/disp32_iprel.S"
	"${KEDR_TEST_ASM_DIR}/long_blocks.S"
)

kedr_test_add_target (${KMODULE_TARGET_NAME})
