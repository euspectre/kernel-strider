set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr")
########################################################################

# Common name of header part of the data file
set(header_data_file "header.data")

# The rules to produce handlers.data and handlers_${group_name}.c.
# [NB] Even if no functions from that particular group are to be 
# intercepted, these two files should still be produced.
# For each function in ${functions} list, ${function}.data file 
# should contain information about the handlers of that function.
function(create_handlers_group group_name functions)
	set(source_file "${CMAKE_CURRENT_BINARY_DIR}/handlers_${group_name}.c")
	set(handlers_data_file "${CMAKE_CURRENT_BINARY_DIR}/handlers.data")
	
	set(functions_data)
	foreach(func ${functions} ${ARGN})
		list(APPEND functions_data "${func}.data")
	endforeach(func ${functions} ${ARGN})
	
	set(data_files_abs)
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${header_data_file}")
		to_abs_path(data_files_abs ${header_data_file} ${functions_data})
	else ()
		to_abs_path(data_files_abs ${functions_data})
	endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${header_data_file}")

	add_custom_command(OUTPUT ${handlers_data_file}
		COMMAND cat ${data_files_abs} > ${handlers_data_file}
		DEPENDS ${data_files_abs}
	)
	
	add_custom_command(OUTPUT ${source_file}
		COMMAND ${KEDR_GEN_TOOL} 
			"${CMAKE_CURRENT_SOURCE_DIR}/templates"
			${handlers_data_file} > ${source_file}
		DEPENDS ${handlers_data_file}
	)
	
	add_custom_target(func_drd_${group_name} ALL
		DEPENDS ${handlers_data_file} ${source_file}
	)
endfunction(create_handlers_group group_name functions)
########################################################################

# Test-related stuff
# The directory where the tests should look for the user-mode helper 
# scripts.
set(KEDR_UMH_DIR "${CMAKE_BINARY_DIR}/core/tests")

# The core module.
set(CORE_MODULE_NAME "kedr_mem_core")
set(CORE_MODULE_DIR "${CMAKE_BINARY_DIR}/core")
########################################################################

# The operations common to different kinds of kernel modules are handled 
# there.
add_subdirectory(common)

if (KEDR_COI_INSTALL_DIR)
    # This plugin handles ordering relationships in various file- and 
	# filesystem-related operations. Needs KEDR COI system.
	add_subdirectory(fs)
else ()
	# Without KEDR COI, the full-fledged handling for cdev operations will 
	# not be built (it is a part of 'fs' plugin). Instead, a simple built-in
	# model for that subsystem will be used.
	add_subdirectory(cdev)
endif (KEDR_COI_INSTALL_DIR)
########################################################################

kedr_test_add_subdirectory(tests)
########################################################################
