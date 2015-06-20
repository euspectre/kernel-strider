set(KMODULE_NAME "kedr_fh_drd_usb")
set(GROUP_NAME "usb")

# These are from "usbcore" module and should always be available.
# Because these functions are not from the kernel proper, 
# kmodule_configure_kernel_functions() cannot be used for the present.
set(functions 
	"usb_register_driver"
	"usb_deregister"
	"usb_submit_urb"
	"usb_kill_urb"
	"usb_poison_urb"
	"usb_kill_anchored_urbs"
	"usb_poison_anchored_urbs"
)

foreach (func ${functions})
	set(val FALSE)
	kmodule_is_exported(${func} "usbcore" val)
	if (val)
		message (STATUS 
			"Looking for ${func} in usbcore - found.")
	else()
		message (STATUS 
			"Looking for ${func} in usbcore - not found.")
		message (FATAL_ERROR 
			"Failed to find a required function: ${func}.")
	endif()
endforeach()

# Rules to generate handlers_usb.c and its prerequisites.
group_create_handlers(${GROUP_NAME} ${functions})

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr_mem_core")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/functions/common/Module.symvers")
kbuild_add_dependencies("kedr_fh_drd_common")

kbuild_add_module(${KMODULE_NAME}
	"handlers_${GROUP_NAME}.c"

# headers
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/functions.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/local_storage.h"
	"${TOP_INCLUDE_DIR}/object_types.h"
	"${CMAKE_BINARY_DIR}/config.h"
)

kedr_install_kmodule(${KMODULE_NAME})
kedr_install_symvers(${KMODULE_NAME})
########################################################################
