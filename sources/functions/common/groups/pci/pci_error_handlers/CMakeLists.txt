# This subgroup provides handlers for the callbacks from 
# struct pci_error_handlers.
# All the callbacks are processed the same way.
set (source_file "cb_pci_error_handlers.c")

check_struct_has_member(
	"struct pci_driver" 
	"err_handler"
	"linux/pci.h"
	pci_driver_has_err_handler)

if (NOT pci_driver_has_err_handler)
	message(FATAL_ERROR 
"struct pci_driver seems not to have 'err_handler' field - please check linux/pci.h"
	)
endif ()

configure_struct_members("struct pci_error_handlers" "linux/pci.h" callbacks
	"error_detected"
	"mmio_enabled"
	"link_reset"
	"slot_reset"
	"resume"
)

set (LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/callbacks.list")
set (KEDR_CALLBACKS_LIST "")
foreach (c ${callbacks})
	set (KEDR_CALLBACKS_LIST "${KEDR_CALLBACKS_LIST}\tfunction = ${c}\n")
endforeach ()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/callbacks.list.in" 
	"${LIST_FILE}"
)

# Generate the source file
add_custom_command(OUTPUT ${source_file}
	COMMAND ${KEDR_GEN_TOOL} 
		"${CMAKE_CURRENT_SOURCE_DIR}/templates"
		"${LIST_FILE}" > ${source_file}
	DEPENDS ${LIST_FILE}
)

group_add_sources(${source_file})
