set(KMODULE_NAME "${TEST_TARGET_NAME}")

if (KEDR_64_BIT)
	set(TEST_ASM_FILE "test_stack_64.S")
else ()
	set(TEST_ASM_FILE "test_stack_32.S")
endif ()

kbuild_add_module(${KMODULE_NAME} 
	"module.c"
	"${TEST_ASM_FILE}"
)

kedr_test_add_target (${KMODULE_NAME})
