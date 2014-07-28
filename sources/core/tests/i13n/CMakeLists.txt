# The directory containing the assembly source files for the tests
if (KEDR_64_BIT)
	set (KEDR_TEST_ASM_DIR 
		"${CMAKE_CURRENT_SOURCE_DIR}/asm/x86-64")
	set (KEDR_TEST_ASM_BINARY_DIR 
		"${CMAKE_CURRENT_BINARY_DIR}/asm/x86-64")
else ()
	set (KEDR_TEST_ASM_DIR 
		"${CMAKE_CURRENT_SOURCE_DIR}/asm/x86-32")
	set (KEDR_TEST_ASM_BINARY_DIR 
		"${CMAKE_CURRENT_BINARY_DIR}/asm/x86-32")
endif ()

# Create the corresponding binary directory for the assembly sources
file(MAKE_DIRECTORY "${KEDR_TEST_ASM_BINARY_DIR}") 

add_subdirectory(basic)
add_subdirectory(target_jtable)

# TODO: Rewrite the tests so that they do not use the information about the
# addresses of the target's loaded sections, then re-enable the tests.
#add_subdirectory(function_lookup)

add_subdirectory(ir_create)
add_subdirectory(transform)
add_subdirectory(null_alloc)
########################################################################
