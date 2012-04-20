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
	to_abs_path(data_files_abs ${header_data_file} ${functions_data})
	
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

add_subdirectory(alloc)
add_subdirectory(realloc)
add_subdirectory(free)
add_subdirectory(lock)
add_subdirectory(unlock)
########################################################################
