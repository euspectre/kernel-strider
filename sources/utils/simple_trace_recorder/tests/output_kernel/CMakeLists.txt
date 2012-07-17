# The part of the output system operating in the kernel mode - test build.
# The main difference from the original build is that the event generator
# module is used for the implementation of the core API rather than 
# kedr_mem_core module.
########################################################################
kbuild_include_directories("${CMAKE_SOURCE_DIR}/utils")
kbuild_include_directories("${KEDR_ST_REC_CONFIG_H_DIR}")

kbuild_use_symbols(
	"${CMAKE_BINARY_DIR}/utils/simple_trace_recorder/tests/event_gen/Module.symvers") 
kbuild_add_dependencies("test_str_event_gen")
kbuild_add_module(${KMODULE_TEST_NAME} 
# sources
	"${CMAKE_SOURCE_DIR}/utils/simple_trace_recorder/kernel/module.c"
	
# headers
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/object_types.h"
	"${KEDR_TR_INCLUDE_DIR}/recorder.h"
	"${KEDR_ST_REC_CONFIG_H_DIR}/kedr_st_rec_config.h"
)
kedr_test_add_target(${KMODULE_TEST_NAME})
########################################################################
