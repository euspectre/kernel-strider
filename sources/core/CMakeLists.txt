# TODO

# Create the core kernel module.
set(kmodule_name "kedr_mem_core")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${kmodule_name}
# Sources:
	"module.c"
	"tid.c"

# Headers
	"tid.h"
)
########################################################################

kedr_install_kmodule(${kmodule_name})
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
