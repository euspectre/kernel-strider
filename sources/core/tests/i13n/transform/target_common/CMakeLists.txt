set(KMODULE_TARGET_NAME "test_ir_transform")

kbuild_add_module(${KMODULE_TARGET_NAME} 
	"module.c"

# Assembly sources with the functions to be used for testing.
# Some of the functions defined there are executable.

# Sources to test phase 1:
	"${KEDR_TEST_ASM_DIR}/base_reg.S"
	"${KEDR_TEST_ASM_DIR}/calls_jumps2.S"

# Sources to test phase 2:
	"${KEDR_TEST_ASM_DIR}/common_type_e.S"
	"${KEDR_TEST_ASM_DIR}/mem_special.S"
	"${KEDR_TEST_ASM_DIR}/strings.S"
	"${KEDR_TEST_ASM_DIR}/locked_updates2.S"
	"${KEDR_TEST_ASM_DIR}/barriers_mem.S"

# The following assembly sources contain functions that are not intended
# to be executed but must be present in the module anyway.
	"${KEDR_TEST_ASM_DIR}/io_mem.S"
)

kedr_test_add_target (${KMODULE_TARGET_NAME})
