cmake_minimum_required (VERSION 2.6)
#######################################################################

# Name of the project and of the application
project(kedr_symbolize_tsan_report)
enable_language(C)
enable_language(CXX)

set(APP_NAME ${PROJECT_NAME})
#######################################################################

kedr_load_install_prefixes()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/symbolize_config.h"
	@ONLY
) 
#######################################################################

include_directories( BEFORE
	"${CMAKE_CURRENT_BINARY_DIR}" 
)

set(APP_SOURCES
	main.cpp
	symbol_info.cpp
	symbol_info.h
	report_processor.cpp
	report_processor.h
)
add_executable(${APP_NAME} ${APP_SOURCES})

set_target_properties (${APP_NAME} PROPERTIES 
	COMPILE_FLAGS "-Wall -Wextra"
)

install(TARGETS ${APP_NAME} 
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC}
)
#######################################################################
