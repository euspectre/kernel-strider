# This subgroup provides handlers for the callbacks from struct pci_driver.
set (source_file "cb_pci_driver.c")

configure_struct_members("struct pci_driver" "linux/pci.h" callbacks
	"probe"
	"remove"
	"suspend"
	"suspend_late"
	"resume_early"
	"resume"
	"shutdown"
	"sriov_configure"
)

if (NOT callbacks)
	message (FATAL_ERROR 
		"No known callbacks found in struct pci_driver.")
endif ()

set (data_files)
foreach (c ${callbacks})
	list (APPEND data_files "${c}.data")
endforeach ()
to_abs_path(data_files_abs ${data_files})

set (func_data "func.data")

add_custom_command(OUTPUT ${func_data}
	COMMAND cat ${data_files_abs} > ${func_data}
	DEPENDS ${data_files_abs}
)

add_custom_command(OUTPUT ${source_file}
	COMMAND ${KEDR_GEN_TOOL} 
		"${CMAKE_CURRENT_SOURCE_DIR}/templates"
		${func_data} > ${source_file}
	DEPENDS ${func_data}
)

group_add_sources(${source_file})
