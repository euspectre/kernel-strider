#######################################################################
# Currently, only GNU C compilers are supported.
if (NOT CMAKE_COMPILER_IS_GNUCC)
    message (FATAL_ERROR 
"Only GNU C compiler is currently supported."
    )
endif ()

include_directories (
    "${CMAKE_SOURCE_DIR}/utils/simple_trace_recorder"
)

add_executable (${RECORDER_NAME} 
	recorder.c 
	"${KEDR_TR_INCLUDE_DIR}/recorder.h"
)

set_target_properties (${RECORDER_NAME} PROPERTIES 
	COMPILE_FLAGS "-Wall -Wextra --std=gnu99 -D_POSIX_SOURCE"
)

install (TARGETS ${RECORDER_NAME} 
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
#######################################################################
