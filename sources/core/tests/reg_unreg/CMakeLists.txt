configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

# test.sh takes a single argument, 'scenario' (integer number). 
# See the description of 'scenario' parameter in test_module/module.c 
# for the details about particular scenarios.
#
# Loading of the test module must succeed in each scenario. The result of 
# the test is output via "test_failed" parameter of the module.

kedr_test_add_script (mem_core.register.01 
    test.sh 0
)

kedr_test_add_script (mem_core.register.02 
    test.sh 1
)

kedr_test_add_script (mem_core.register.03 
    test.sh 2
)

add_subdirectory (test_module)
