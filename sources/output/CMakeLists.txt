# TODO
kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(kernel)
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
