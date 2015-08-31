# A module with a split function. The instrumentation system should
# detect that.
set(KMODULE_TARGET_NAME "test_ir_create_split_func")

kbuild_add_module(${KMODULE_TARGET_NAME} 
	"module.c"

# Assembly sources with the functions to be used for testing
	"${KEDR_TEST_ASM_DIR}/split_func.S"
)

kedr_test_add_target (${KMODULE_TARGET_NAME})
