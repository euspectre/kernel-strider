set(TEST_ANN_FILES_DIR "${CMAKE_SOURCE_DIR}/core/annotations")

kbuild_add_definitions("-DCONFIG_KEDR_ANNOTATIONS=1")
kbuild_include_directories("${TEST_ANN_FILES_DIR}")

set(test_src
	"module.c"
	"${TEST_ANN_FILES_DIR}/kedr_annotations.h"
)
if (NOT KEDR_ANNOTATIONS_ENABLED)
	# If KEDR annotations are not enabled in the kernel itself,
	# build their implementations into the test module.
	# Otherwise, the implementation from the kernel should be used.
	list(APPEND test_src "${TEST_ANN_FILES_DIR}/kedr_annotations.c")
endif (NOT KEDR_ANNOTATIONS_ENABLED)

kbuild_add_module(${KMODULE_TEST_NAME} ${test_src})
kedr_test_add_target(${KMODULE_TEST_NAME})
