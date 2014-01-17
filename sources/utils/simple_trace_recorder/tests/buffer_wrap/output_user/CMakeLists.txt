# The user-space part of the output system, the test build.
include_directories(
    "${CMAKE_SOURCE_DIR}/utils"
    "${KEDR_ST_REC_CONFIG_H_DIR}"
)

add_executable(${RECORDER_TEST_NAME} 
	"${CMAKE_SOURCE_DIR}/utils/simple_trace_recorder/user/recorder.c"
	"${KEDR_TR_INCLUDE_DIR}/recorder.h"
	"${KEDR_ST_REC_CONFIG_H_DIR}/kedr_st_rec_config.h"
)

set_target_properties(${RECORDER_TEST_NAME} PROPERTIES 
	COMPILE_FLAGS "-Wall -Wextra --std=gnu99 -D_POSIX_SOURCE"
)

kedr_test_add_target(${RECORDER_TEST_NAME})
#######################################################################
