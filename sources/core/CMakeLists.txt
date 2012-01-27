# TODO

# Create the core kernel module.
set(kmodule_name "kedr_mem_core")
kbuild_add_module(${kmodule_name}
	"module.c"
)
########################################################################

kedr_install_kmodule(${kmodule_name})
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
