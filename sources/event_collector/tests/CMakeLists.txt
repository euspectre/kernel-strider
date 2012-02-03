set(core_stub_module_name "test_core_stub")
set(handler_stub_module_name "test_handler_stub")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(core_stub)
add_subdirectory(handler_stub)

add_subdirectory(test_simple)
add_subdirectory(test_with_handler)
