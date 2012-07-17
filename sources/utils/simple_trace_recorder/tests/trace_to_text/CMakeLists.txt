set(APP_NAME "test_trace_to_text")

include_directories (
    "${CMAKE_SOURCE_DIR}/utils/simple_trace_recorder"
    "${CMAKE_SOURCE_DIR}/include"
)

add_executable (${APP_NAME} 
	converter.cpp 
	"${KEDR_TR_INCLUDE_DIR}/recorder.h"
	"${CMAKE_SOURCE_DIR}/include/kedr/object_types.h"
)

set_target_properties (${APP_NAME} PROPERTIES 
	COMPILE_FLAGS "-Wall -Wextra"
)

kedr_test_add_target(${APP_NAME})
#######################################################################
