# This group handles operations with PCI devices.

kmodule_configure_kernel_functions(functions OPTIONAL
	"__pci_register_driver"
	"pci_unregister_driver"
)

check_struct_has_member(
	"struct pci_driver" 
	"driver"
	"linux/pci.h"
	pci_driver_has_driver)

if (NOT pci_driver_has_driver)
	message(FATAL_ERROR 
"struct pci_driver seems not to have 'driver' field - please check linux/pci.h"
	)
endif ()

group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################

add_subdirectory(pci_driver)
add_subdirectory(pci_error_handlers)
############################################################################
