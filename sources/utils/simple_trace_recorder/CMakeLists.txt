kedr_load_install_prefixes()
set(KEDR_TR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(KMODULE_NAME  "kedr_simple_trace_recorder")
set(RECORDER_NAME "kedr_st_recorder")

add_subdirectory(kernel)
add_subdirectory(user)
