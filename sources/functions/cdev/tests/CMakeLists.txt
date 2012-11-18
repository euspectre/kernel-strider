# This test checks the basic functionality of the plugin that handles some
# operations with the character devices.

set(EXPR_SIGNAL_PRE 
	"TID=0x[0-9a-f]+ SIGNAL COMMON PRE pc=[0-9a-f]+ id=0x[0-9a-f]+")
set(EXPR_SIGNAL_POST 
	"TID=0x[0-9a-f]+ SIGNAL COMMON POST pc=[0-9a-f]+ id=0x[0-9a-f]+")
set(EXPR_SIGNAL "${EXPR_SIGNAL_PRE}${EXPR_SIGNAL_POST}")

set(EXPR_WAIT_PRE 
	"TID=0x[0-9a-f]+ WAIT COMMON PRE pc=[0-9a-f]+ id=0x[0-9a-f]+")
set(EXPR_WAIT_POST 
	"TID=0x[0-9a-f]+ WAIT COMMON POST pc=[0-9a-f]+ id=0x[0-9a-f]+")
set(EXPR_WAIT "${EXPR_WAIT_PRE}${EXPR_WAIT_POST}")

set(EXPR_INIT
	"(${EXPR_SIGNAL}){5}.*(${EXPR_SIGNAL}){5}")

# At least open() and write() must be detected and processed appropriately.
set(EXPR_OPS
	"(${EXPR_WAIT}.*${EXPR_SIGNAL}.*){2,}")

set(EXPR_EXIT
	"(${EXPR_WAIT}){10}.*(${EXPR_WAIT}){5}.*(${EXPR_WAIT}){5}")

set(TEST_MATCH_EXPR
	"${EXPR_INIT}.*${EXPR_OPS}.*${EXPR_EXIT}")

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)
kedr_test_add_script (functions.cdev.01
	test.sh
)

kedr_test_add_script (functions.cdev.02
	test.sh with-common
)
