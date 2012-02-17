# Handler stub module is used in tests for event collector module.
set(handler_stub_module_name "test_handler_stub" CACHE INTERNAL
	"Name of the handler stub module")
set(handler_stub_module_dir "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL
	"Directory where handler stub module is placed")
set(handler_stub_include_dir "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL
	"Directory where include file of handler stub module is placed")

set(KMODULE_NAME ${handler_stub_module_name})
kbuild_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../../Module.symvers")

kbuild_add_module(${KMODULE_NAME}
	"handler_stub.c"
)

kedr_test_add_target(${KMODULE_NAME})