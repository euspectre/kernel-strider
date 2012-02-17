set(KMODULE_NAME "event_collector_test_wh_module")

kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../../Module.symvers")

kbuild_use_symbols("${core_stub_module_dir}/Module.symvers")
kbuild_add_dependencies(${core_stub_module_name})
kbuild_include_directories(${core_stub_include_dir})

kbuild_use_symbols("${handler_stub_module_dir}/Module.symvers")
kbuild_add_dependencies(${handler_stub_module_name})
kbuild_include_directories(${handler_stub_include_dir})


kbuild_add_module(${KMODULE_NAME}
	"test_module.c"
)

kedr_test_add_target(${KMODULE_NAME})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_with_handler.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_with_handler.sh"
    @ONLY)

kedr_test_add_script("event_collector.test_with_handler.01"
    "test_with_handler.sh")