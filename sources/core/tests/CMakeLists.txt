set(CORE_MODULE_DIR "${CMAKE_BINARY_DIR}/core")
set(CORE_MODULE_NAME "${KMODULE_NAME}")

kbuild_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(simple_ins_rm)
add_subdirectory(reg_unreg)
add_subdirectory(callbacks)
add_subdirectory(ls_allocator)
add_subdirectory(sections)
########################################################################
