cmake_minimum_required (VERSION 2.6)
#######################################################################

# Name of the subproject and of the application
project (tsan_process_trace)
enable_language (C)
enable_language(CXX)
#######################################################################

include_directories (
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
)

set(APP_SOURCES
	main.cpp
	process_trace.h
	module_info.h
	module_info.cpp
	rc_ptr.h
	trace_processor.h
	trace_processor.cpp
)
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# _GNU_SOURCE is needed because at least getopt_long() is GNU extension.
set_target_properties(${PROJECT_NAME} PROPERTIES
	COMPILE_FLAGS "-Wall -Wextra -D_GNU_SOURCE"
)

target_link_libraries(${PROJECT_NAME} "elf" "dw")
#######################################################################

install(TARGETS ${PROJECT_NAME}
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
#######################################################################
