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

# Adds the tests for those functions from ${functions_to_test} list that
# are actually available in the system (i.e. are listed in ${functions}).
# The tests check that the appropriate events are generated for the calls 
# to these functions. 
# The actual testing is performed using "test.sh" script in the tests'
# directory for the group, the name of the function being the argument of
# that script.
function(add_tests_for_group group_name functions_to_test)
	set(test_data_file "${CMAKE_CURRENT_BINARY_DIR}/test.data")
	set(test_header_data_file "header.data")
	set(functions_triggers)
	set(test_module "test_func_drd_${group_name}")
	set(template_dir "${CMAKE_SOURCE_DIR}/functions/groups/tests/templates")

	foreach(func ${functions_to_test} ${ARGN})
		list(FIND functions ${func} func_index)
		if (NOT (func_index EQUAL -1))
			list(APPEND functions_triggers "${func}.trigger")
			
			configure_file (
				"${CMAKE_SOURCE_DIR}/functions/groups/tests/test.sh.in"
				"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
				@ONLY
			)
			kedr_test_add_script (functions.handle.${func}.01
			    "test.sh" ${func}
			)
		endif()
	endforeach(func ${functions_to_test} ${ARGN})

	to_abs_path(data_files_abs ${test_header_data_file} 
		${functions_triggers})
	
	add_custom_command(OUTPUT ${test_data_file}
		COMMAND cat ${data_files_abs} > ${test_data_file}
		DEPENDS ${data_files_abs}
	)
	
	add_custom_command(OUTPUT "module.c"
		COMMAND ${KEDR_GEN_TOOL} "${template_dir}" 
			"${test_data_file}" > "module.c"
		DEPENDS "${test_data_file}"
	)
	
	kbuild_add_module(${test_module}
		"module.c"
	)  
	kedr_test_add_target(test_func_drd_${group_name})
endfunction(add_tests_for_group group_name functions_to_test)
########################################################################

foreach(group ${FUNCTION_GROUPS})
	add_subdirectory(${group})
endforeach(group ${FUNCTION_GROUPS})
########################################################################
