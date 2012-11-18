set(KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/utils/st_recorder/multiple_targets")
set(KEDR_ST_REC_DEBUGFS_DIR "${KEDR_TEST_TEMP_DIR}/debug")

set(KEDR_ST_REC_CONFIG_H_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(RECORDER_TEST_NAME "test_st_recorder2")
set(KEDR_ST_REC_KMODULE_NAME "kedr_simple_trace_recorder")

configure_file(
	"${CMAKE_SOURCE_DIR}/utils/simple_trace_recorder/kedr_st_rec_config.h.in" 
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_st_rec_config.h")

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test_multi.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_multi.sh"
	@ONLY
)

kedr_test_add_script (utils.simple_trace_recorder.02
	test_multi.sh
)

add_subdirectory(output_user)
