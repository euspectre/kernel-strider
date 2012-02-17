set(KMODULE_TEST_NAME "test_core_hooks")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/hooks.h"
	"${CMAKE_SOURCE_DIR}/core/hooks.h"
)
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/i13n.h"
	"${CMAKE_SOURCE_DIR}/core/i13n.h"
)

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_module(${KMODULE_TEST_NAME} 
	"module.c"
	"hooks.h"
	"i13n.h"
)

kedr_test_add_target (${KMODULE_TEST_NAME})
