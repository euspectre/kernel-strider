cmake_minimum_required (VERSION 2.6)
#######################################################################

# Name of the project and of the application
project(symbolize_tsan_report)
enable_language(C)
enable_language(CXX)

set(APP_NAME ${PROJECT_NAME})
#######################################################################

# Prohibit a common type of an in-source build.
# Note that building in a subdirectory in the source tree is still allowed 
# as it can be convenient.
string (COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" in_source)
if (in_source)
	message (FATAL_ERROR 
"It is not allowed to build the project in its top source directory."
	)
endif () 
#######################################################################

# Make "Release" the default build type
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE "Release")
endif ()
message (STATUS "${PROJECT_NAME}: Build type is \"${CMAKE_BUILD_TYPE}\"")

#######################################################################

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/config.h"
	@ONLY
) 
#######################################################################
include_directories(
	"${CMAKE_CURRENT_BINARY_DIR}" 
)

set(APP_SOURCES
	main.cpp
	symbol_info.cpp
	symbol_info.h
	report_processor.cpp
	report_processor.h
)
add_executable (${APP_NAME} ${APP_SOURCES})
#######################################################################
