# Currently, only GNU C compilers are supported.
if (NOT CMAKE_COMPILER_IS_GNUCC)
    message (FATAL_ERROR 
"Only GNU C compiler is currently supported."
    )
endif ()

kedr_load_install_prefixes()
set(KEDR_TR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(KMODULE_NAME  "kedr_simple_trace_recorder")
set(RECORDER_NAME "kedr_st_recorder")
set(KEDR_ST_REC_KMODULE_NAME "${KMODULE_NAME}")

set(KEDR_ST_REC_CONFIG_H_DIR 
	"${CMAKE_CURRENT_BINARY_DIR}")

# The user-space part of the output system need that.
set(KEDR_ST_REC_DEBUGFS_DIR "/sys/kernel/debug")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_st_rec_config.h.in" 
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_st_rec_config.h")

add_subdirectory(kernel)
add_subdirectory(user)
add_subdirectory(converter)
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################