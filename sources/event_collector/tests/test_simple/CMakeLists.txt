set(KMODULE_NAME "event_collector_test_simple_module")

kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../core_stub/Module.symvers")
kbuild_add_dependencies(${core_stub_module_name})

kbuild_add_module(${KMODULE_NAME}
	"test_module.c"
)

kedr_test_add_target(${KMODULE_NAME})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_simple.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_simple.sh"
    @ONLY)

kedr_test_add_script("event_collector.test_simple.01"
    "test_simple.sh")