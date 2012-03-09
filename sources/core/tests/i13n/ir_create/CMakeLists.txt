kedr_load_test_prefixes()
set (KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/ir_create")

# TODO: uncomment when the targets are prepared
#configure_file (
#  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
#  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
#  @ONLY
#)

#kedr_test_add_script (mem_core.i13n.ir_create.NN
#    test.sh <...> <...>
#)

add_subdirectory(accessor)
# add_subdirectory(targets)
