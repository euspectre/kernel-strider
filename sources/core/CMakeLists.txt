# TODO

# Create the core kernel module.
set(KMODULE_NAME "kedr_mem_core")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME}
# Sources:
	"module.c"
	"tid.c"

# Headers
	"core_impl.h"
	"tid.h"
)
########################################################################

kedr_install_kmodule(${KMODULE_NAME})
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
