kedr_load_install_prefixes()

# The path where the examples will be installed
set (KEDR_EXAMPLE_PATH "${KEDR_INSTALL_PREFIX_EXAMPLES}")

add_subdirectory(buggy01)
add_subdirectory(buggy01_annotated)
add_subdirectory(test_buggy01)
