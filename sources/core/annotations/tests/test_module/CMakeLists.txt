set(TEST_ANN_FILES_DIR "${CMAKE_SOURCE_DIR}/core/annotations")

kbuild_include_directories("${TEST_ANN_FILES_DIR}")
kbuild_add_module(${KMODULE_TEST_NAME} 
	"module.c"
	"${TEST_ANN_FILES_DIR}/kedr_annotations.c"
	"${TEST_ANN_FILES_DIR}/kedr_annotations.h"
)

kedr_test_add_target (${KMODULE_TEST_NAME})
