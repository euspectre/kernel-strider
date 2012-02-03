set(KMODULE_NAME ${handler_stub_module_name})

kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../../Module.symvers")

kbuild_add_module(${KMODULE_NAME}
	"handler_stub.c"
)

kedr_test_add_target(${KMODULE_NAME})