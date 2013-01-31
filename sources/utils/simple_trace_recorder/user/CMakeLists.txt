include_directories(
    "${CMAKE_SOURCE_DIR}/utils"
    "${KEDR_ST_REC_CONFIG_H_DIR}"
)

add_executable(${RECORDER_NAME} 
	recorder.c 
	"${KEDR_TR_INCLUDE_DIR}/recorder.h"
	"${KEDR_ST_REC_CONFIG_H_DIR}/kedr_st_rec_config.h"
)

set_target_properties(${RECORDER_NAME} PROPERTIES 
	COMPILE_FLAGS "-Wall -Wextra --std=gnu99 -D_POSIX_SOURCE -D_FILE_OFFSET_BITS=64"
)

install(TARGETS ${RECORDER_NAME} 
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
#######################################################################
