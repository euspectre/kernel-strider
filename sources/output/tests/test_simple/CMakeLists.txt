set(KMODULE_NAME "trace_sender_test_simple_module")
set(EXECUTABLE_NAME "trace_sender_test_simple_checker")

kbuild_use_symbols("${core_stub_module_dir}/Module.symvers")
kbuild_use_symbols("${trace_sender_module_dir}/Module.symvers")
kbuild_add_dependencies(${core_stub_module_name})
kbuild_include_directories(${core_stub_include_dir})

kbuild_add_module(${KMODULE_NAME}
	"module.c"
)

kedr_test_add_target(${KMODULE_NAME})

add_executable(${EXECUTABLE_NAME}
	"checker.cpp")

target_link_libraries(${EXECUTABLE_NAME} ${kedr_trace_reader_name})

kedr_test_add_target(${EXECUTABLE_NAME})

add_custom_command(OUTPUT "module.c"
	COMMAND "${KEDR_GEN_TOOL}"
	"${CMAKE_CURRENT_SOURCE_DIR}/templates/module.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/test.data" >
	"${CMAKE_CURRENT_BINARY_DIR}/module.c"
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test.data")

add_custom_command(OUTPUT "checker.cpp"
	COMMAND "${KEDR_GEN_TOOL}"
	"${CMAKE_CURRENT_SOURCE_DIR}/templates/checker.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/test.data" >
	"${CMAKE_CURRENT_BINARY_DIR}/checker.cpp"
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test.data")


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_simple.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_simple.sh"
    @ONLY)

kedr_test_add_script("trace_sender.test_simple.01"
    "test_simple.sh")
