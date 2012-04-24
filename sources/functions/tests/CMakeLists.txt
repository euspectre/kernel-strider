# The directory where the tests should look for the user-mode helper 
# scripts.
set(KEDR_UMH_DIR "${CMAKE_BINARY_DIR}/core/tests")

# The core module.
set(CORE_MODULE_NAME "kedr_mem_core")
set(CORE_MODULE_DIR "${CMAKE_BINARY_DIR}/core")

add_subdirectory(args)
########################################################################
