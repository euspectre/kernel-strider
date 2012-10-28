set(KMODULE_NAME "kedr_fh_drd_cdev")
set(GROUP_NAME "cdev")

# It is needed to intercept registration and deregistration of the 
# callbacks for file operations with the character devices.
kmodule_configure_kernel_functions(functions OPTIONAL
	"cdev_add"
	"cdev_del"
)

list(LENGTH functions num_funcs)
if (num_funcs)
	# Rules to generate handlers_cdev.c and its prerequisites.
	create_handlers_group(${GROUP_NAME} ${functions})
	
	kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
	kbuild_add_dependencies("kedr_mem_core")
	kbuild_add_module(${KMODULE_NAME}
		"handlers_${GROUP_NAME}.c"

	# headers
		"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
		"${TOP_INCLUDE_DIR}/kedr_mem/functions.h"
		"${TOP_INCLUDE_DIR}/kedr_mem/local_storage.h"
		"${TOP_INCLUDE_DIR}/object_types.h"
		"${CMAKE_BINARY_DIR}/config.h"
	)
	
	add_dependencies(${KMODULE_NAME} func_drd_${GROUP_NAME})
	kedr_install_kmodule(${KMODULE_NAME})
	kedr_install_symvers(${KMODULE_NAME})
endif (num_funcs)
########################################################################
