# TODO

# Create the core kernel module.
set(kmodule_name "kernel_strider")
kbuild_add_module(${kmodule_name}
	"module.c"
)
########################################################################

kedr_install_kmodule(${kmodule_name})
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
