kedr_load_test_prefixes()

#set(EXPR_SIGNAL_PRE 
#	"TID=0x[0-9a-f]+ SIGNAL COMMON PRE pc=[0-9a-f]+ id=")
#set(EXPR_SIGNAL_POST 
#	"TID=0x[0-9a-f]+ SIGNAL COMMON POST pc=[0-9a-f]+ id=0x[0-9a-f]+")
#set(EXPR_SIGNAL "${EXPR_SIGNAL_PRE}(0x[0-9a-f]+)${EXPR_SIGNAL_POST}")
#
#set(EXPR_WAIT_PRE 
#	"TID=0x[0-9a-f]+ WAIT COMMON PRE pc=[0-9a-f]+ id=0x[0-9a-f]+")
#set(EXPR_WAIT_POST 
#	"TID=0x[0-9a-f]+ WAIT COMMON POST pc=[0-9a-f]+ id=0x[0-9a-f]+")
#set(EXPR_WAIT "${EXPR_WAIT_PRE}${EXPR_WAIT_POST}")

set(TEST_MATCH_EXPR "")
set(TEST_CHECKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/check_trace.awk")

add_tests_for_group(${GROUP_NAME} ${functions})
########################################################################
