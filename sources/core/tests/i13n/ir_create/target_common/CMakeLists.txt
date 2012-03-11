set(KMODULE_TARGET_NAME "test_ir_create_common")

kbuild_add_module(${KMODULE_TARGET_NAME} 
	"module.c"

# Assembly sources with the functions to be used for testing
	"${KEDR_TEST_ASM_DIR}/barriers_cpuid.S"
	"${KEDR_TEST_ASM_DIR}/jump_tables.S"
)

kedr_test_add_target (${KMODULE_TARGET_NAME})
