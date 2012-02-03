set(KMODULE_NAME ${core_stub_module_name})

kbuild_add_module(${KMODULE_NAME}
	"core_stub.c"
)

kedr_test_add_target(${KMODULE_NAME})