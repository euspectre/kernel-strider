set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr")
set(KEDR_ST_REC_CONFIG_H_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(KMODULE_TEST_NAME "test_simple_trace_recorder")
set(RECORDER_TEST_NAME "test_st_recorder")
set(EVENT_GEN_NAME "test_str_event_gen")
set(KEDR_ST_REC_KMODULE_NAME "${KMODULE_TEST_NAME}")

if (KEDR_64_BIT)
	set(KEDR_TEST_EXPECTED_TRACE 
		"${CMAKE_CURRENT_SOURCE_DIR}/expected64.txt")
else ()
	set(KEDR_TEST_EXPECTED_TRACE 
		"${CMAKE_CURRENT_SOURCE_DIR}/expected32.txt")
endif ()

kedr_load_test_prefixes()
set(KEDR_TEST_TEMP_DIR "${KEDR_TEST_PREFIX_TEMP}/utils/st_recorder")
set(KEDR_ST_REC_DEBUGFS_DIR "${KEDR_TEST_TEMP_DIR}/debug")

configure_file(
	"${CMAKE_SOURCE_DIR}/utils/simple_trace_recorder/kedr_st_rec_config.h.in" 
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_st_rec_config.h")

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)

kedr_test_add_script (utils.simple_trace_recorder.01
	test.sh
)

add_subdirectory(event_gen)
add_subdirectory(output_kernel)
add_subdirectory(output_user)
add_subdirectory(trace_to_text)
