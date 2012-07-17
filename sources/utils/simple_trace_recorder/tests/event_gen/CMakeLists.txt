# The part of the output system operating in the kernel mode.
########################################################################
set(KMODULE_TEST_NAME ${EVENT_GEN_NAME})
set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr")
kbuild_include_directories("${CMAKE_SOURCE_DIR}/utils")

kbuild_add_module(${KMODULE_TEST_NAME} 
# sources
	module.c
	
# headers
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/object_types.h"
	"${KEDR_TR_INCLUDE_DIR}/recorder.h"
)
kedr_test_add_target(${KMODULE_TEST_NAME})
########################################################################
