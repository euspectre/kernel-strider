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
kedr_test_add_script (mem_core.i13n.basic.02
	test.sh 2 "process_stack_accesses"
)

# A simple test on a target module with jump tables.
configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test_jtable.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_jtable.sh"
	@ONLY
)
kedr_test_add_script (mem_core.i13n.basic.03
	test_jtable.sh
)
