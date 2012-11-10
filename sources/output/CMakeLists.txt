# TODO
kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

set(trace_sender_module_name "kedr_trace_sender")
set(trace_sender_module_dir "${CMAKE_CURRENT_BINARY_DIR}/kernel/")
set(meta_template_shipped_name 
	"ctf_meta_template.o_shipped")
set(meta_template_shipped_file 
	"${CMAKE_CURRENT_BINARY_DIR}/kernel/ctf_meta_template.o_shipped")

add_subdirectory(kernel)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(user)
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
