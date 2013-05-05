set(cmake_this_dir "${CMAKE_SOURCE_DIR}/cmake/modules/")

#Create rule for obtain one file by copying another one
function(rule_copy_file target_file source_file)
    add_custom_command(OUTPUT ${target_file}
                    COMMAND cp -p ${source_file} ${target_file}
                    DEPENDS ${source_file}
                    )
endfunction(rule_copy_file target_file source_file)

#Create rule for obtain file in binary tree by copiing it from source tree
function(rule_copy_source rel_source_file)
    rule_copy_file(${CMAKE_CURRENT_BINARY_DIR}/${rel_source_file} ${CMAKE_CURRENT_SOURCE_DIR}/${rel_source_file})
endfunction(rule_copy_source rel_source_file)

# to_abs_path(output_var path [...])
#
# Convert relative path of file to absolute path:
# use path in source tree, if file already exist there.
# otherwise use path in binary tree.
# If initial path already absolute, return it.
function(to_abs_path output_var)
    set(result)
    foreach(path ${ARGN})
        string(REGEX MATCH "^/" _is_abs_path ${path})
        if(_is_abs_path)
            list(APPEND result ${path})
        else(_is_abs_path)
            file(GLOB to_abs_path_file 
                "${CMAKE_CURRENT_SOURCE_DIR}/${path}"
            )
            if(NOT to_abs_path_file)
                set (to_abs_path_file "${CMAKE_CURRENT_BINARY_DIR}/${path}")
            endif(NOT to_abs_path_file)
            list(APPEND result ${to_abs_path_file})
        endif(_is_abs_path)
    endforeach(path ${ARGN})
    set("${output_var}" ${result} PARENT_SCOPE)
endfunction(to_abs_path output_var path)

#is_path_inside_dir(output_var dir path)
#
# Set output_var to true if path is absolute path inside given directory.
# (!) path should be absolute.
macro(is_path_inside_dir output_var dir path)
    file(RELATIVE_PATH _rel_path ${dir} ${path})
    string(REGEX MATCH "^\\.\\." _is_not_inside_dir ${_rel_path})
    if(_is_not_inside_dir)
        set(${output_var} "FALSE")
    else(_is_not_inside_dir)
        set(${output_var} "TRUE")
    endif(_is_not_inside_dir)
endmacro(is_path_inside_dir output_var dir path)
########################################################################

function(check_libelf_devel)
	# libelf and its development files are required for trace processing
	# as well as for the tests
	set(checking_message "Checking for libelf development files")
	message(STATUS "${checking_message}")

	if(libelf_headers)
		set(checking_message "${checking_message} [cached] - done")
	else(libelf_headers)
		try_compile(libelf_compile_result # Result variable
			"${CMAKE_BINARY_DIR}/check_libelf_devel/libelf_check" # Binary dir
			"${CMAKE_SOURCE_DIR}/cmake/other_sources/libelf_check.c" # Source file
			CMAKE_FLAGS "-DLINK_LIBRARIES:string=elf"
			OUTPUT_VARIABLE libelf_compile_out)
		if(NOT libelf_compile_result)
			message(STATUS "${checking_message} - not found")
			message(FATAL_ERROR
				"Unable to find the development files for libelf library.")
		endif(NOT libelf_compile_result)

		set(libelf_headers "FOUND" CACHE INTERNAL "Whether libelf headers are installed")
		set(checking_message "${checking_message} - done")
	endif(libelf_headers)

	message(STATUS "${checking_message}")
endfunction(check_libelf_devel)

function(check_libdw_devel)
	# libdw and its development files are required for trace processing.
	set(checking_message "Checking for libdw development files")
	message(STATUS "${checking_message}")

	if(libdw_headers)
		set(checking_message "${checking_message} [cached] - done")
	else(libdw_headers)
		try_compile(libdw_compile_result # Result variable
			"${CMAKE_BINARY_DIR}/check_libdw_devel/libdw_check" # Binary dir
			"${CMAKE_SOURCE_DIR}/cmake/other_sources/libdw_check.c" # Source file
			CMAKE_FLAGS "-DLINK_LIBRARIES:string=elf;dw"
			OUTPUT_VARIABLE libdw_compile_out)
		if(NOT libdw_compile_result)
			message(STATUS "${checking_message} - not found")
			message(FATAL_ERROR
				"Unable to find the development files for libdw library.")
		endif(NOT libdw_compile_result)

		set(libdw_headers "FOUND" CACHE INTERNAL "Whether libdw headers are installed")
		set(checking_message "${checking_message} - done")
	endif(libdw_headers)

	message(STATUS "${checking_message}")
endfunction(check_libdw_devel)

########################################################################
# Test-related macros
########################################################################

# When we are building KEDR for another system (cross-build), testing is
# disabled. This is because the tests need the build tree.
# In the future, the tests could be prepared that need only the installed 
# components of KEDR. It could be a separate test suite.

# This macro enables testing support and performs other initialization tasks.
# It should be used in the top-level CMakeLists.txt file before 
# add_subdirectory () calls.
macro (kedr_test_init)
	if (NOT CMAKE_CROSSCOMPILING)
	    enable_testing ()
	    add_custom_target (check 
	        COMMAND ${CMAKE_CTEST_COMMAND}
	    )
	    add_custom_target (build_tests)
	    add_dependencies (check build_tests)
	endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_init)

# Use this macro to specify an additional target to be built before the tests
# are executed.
macro (kedr_test_add_target target_name)
	if (NOT CMAKE_CROSSCOMPILING)
	    set_target_properties (${target_name}
	        PROPERTIES EXCLUDE_FROM_ALL true
	    )
	    add_dependencies (build_tests ${target_name})
	endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_add_target target_name)

# This function adds a test script (a Bash script, actually) to the set of
# tests for the package. The script may reside in current source or binary 
# directory (the source directory is searched first).
function (kedr_test_add_script test_name script_file)
	if (NOT CMAKE_CROSSCOMPILING)
	    to_abs_path (TEST_SCRIPT_FILE ${script_file})
	        
	    add_test (${test_name}
	        /bin/bash ${TEST_SCRIPT_FILE} ${ARGN}
	    )
	endif (NOT CMAKE_CROSSCOMPILING)
endfunction (kedr_test_add_script)

function (kedr_test_add test_name app_file)
	if (NOT CMAKE_CROSSCOMPILING)
	    to_abs_path (TEST_APP_FILE ${app_file})
	        
	    add_test (${test_name} ${TEST_APP_FILE} ${ARGN})
	endif (NOT CMAKE_CROSSCOMPILING)
endfunction (kedr_test_add)

# Use this macro instead of add_subdirectory() for the subtrees related to 
# testing of the package.

# We could use other kedr_*test* macros to disable the tests when 
# cross-building, but the rules of Kbuild system (concerning .symvers,
# etc.) still need to be disabled explicitly. So it is more reliable to 
# just turn off each add_subdirectory(tests) in this case.
macro (kedr_test_add_subdirectory subdir)
	if (NOT CMAKE_CROSSCOMPILING)
		add_subdirectory(${subdir})
	endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_add_subdirectory subdir)
########################################################################

# Looks for ThreadSanitizer offline and sets '${output_var}' accordingly.
# The format is the same as for 'find_program'.
function (find_tsan output_var)
	set (tsan_app)
	set (checking_message "Checking for ThreadSanitizer offline")
	message (STATUS "${checking_message}")

	# First look for <arch>-linux-debug-ts_offline, then - for ts_offline.
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set (tsan_app_name "amd64-linux-debug-ts_offline")
	else ()
		set (tsan_app_name "x86-linux-debug-ts_offline")
	endif()

	find_program(tsan_app ${tsan_app_name})
	if (NOT tsan_app)
		find_program(tsan_app "ts_offline")
	endif ()

	if (tsan_app)
		message (STATUS "${checking_message} - ${tsan_app}")
	else ()
		message (STATUS "${checking_message} - not found")
	endif ()
	
	set("${output_var}" "${tsan_app}" PARENT_SCOPE)
endfunction (find_tsan output_var) 
########################################################################

# check_struct_has_member()
# Checks if there is a member 'member' in the struct of type 'type'
# defined in the given header file in the kernel.
# The result will be available in 'result_var'.
########################################################################
function (check_struct_has_member type member header result_var)
	set (checking_message "Checking if '${type}' contains '${member}'")
	message (STATUS "${checking_message}")

	set (has_var_impl "${type}_has_${member}")
	string (REPLACE " " "_" has_var "${has_var_impl}")
	
	if (DEFINED ${has_var})
		message (STATUS "${checking_message} [cached] - ${${has_var}}")
	else ()
		set (workdir "${CMAKE_BINARY_DIR}/check_struct_has_member/${has_var}")
		file (MAKE_DIRECTORY "${workdir}")
		
		# Generate the source file for try_compile()
		execute_process(COMMAND sh 
			"${cmake_this_dir}/kmodule_files/scripts/gen_check_member_code.sh"
			"${type}" "${member}" "${header}"
			OUTPUT_FILE "${workdir}/module.c"
			RESULT_VARIABLE res)

		if (NOT res EQUAL 0)
			message (FATAL_ERROR 
				"Failed to generate code to check if '${type}' contains '${member}'."
				"Exit status of the script: ${res}.")
		endif ()
		
		kmodule_try_compile(compile_result
			"${workdir}/build" # Binary dir
			"${workdir}/module.c" # Source file
			OUTPUT_VARIABLE compile_out)
		if (compile_result)
			set ("${result_var}" "yes")
		else()
			set ("${result_var}" "no")
		endif()
		
		set ("${has_var}" "${${result_var}}" CACHE INTERNAL "Does '${type}' have member '${member}'?")
		set ("${result_var}" "${${result_var}}" PARENT_SCOPE)
		message (STATUS "${checking_message} - ${${result_var}}")
	endif ()
	
	set ("${result_var}" "${${has_var}}" PARENT_SCOPE)
endfunction ()

