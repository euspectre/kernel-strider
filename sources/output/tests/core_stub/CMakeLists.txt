# Core stub module is used in tests for event collector module.
set(core_stub_module_name "test_core_stub" CACHE INTERNAL
	"Name of the core stub module")
set(core_stub_module_dir "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL
	"Directory where core stub module is placed")

set(core_stub_include_dir "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL
	"Directory where include file of core stub module is placed")

set(KMODULE_NAME ${core_stub_module_name})
kbuild_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

kbuild_add_module(${KMODULE_NAME}
	"core_stub.c"
)

kedr_test_add_target(${KMODULE_NAME})