# This test checks the basic functionality of the core as a whole.
# It uses the core to process the sample target and checks a few 
# operations with the target. The core is loaded once but the target
# is loaded and unloaded N times, N being the parameter of the test.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_add_script (mem_core.i13n.basic.01
    test.sh 3
)
