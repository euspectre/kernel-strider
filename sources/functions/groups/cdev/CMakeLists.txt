set(GROUP_NAME "cdev")

# It is needed to intercept registration and deregistration of the 
# callbacks for file operations with the character devices.
kmodule_configure_kernel_functions(functions OPTIONAL
	"cdev_add"
	"cdev_del"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
