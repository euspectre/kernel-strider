# List of include files that do not need configuring and should be 
# copied to the build tree as is.
# [NB] When you add a header to the "include" subtree of the source 
# tree, do not forget to add it to the list below.
set(KEDR_INCLUDE_FILES_SRC
	object_types.h
	kedr_mem/core_api.h
	kedr_mem/local_storage.h
	kedr_mem/block_info.h
	kedr_mem/functions.h
	output/event_collector.h
	ctf_reader/ctf_reader.h
	ctf_reader/ctf_meta.h
	ctf_reader/ctf_type.h
	ctf_reader/ctf_var.h
	ctf_reader/ctf_var_place.h
	ctf_reader/ctf_context.h
	ctf_reader/ctf_hash.h
	ctf_reader/ctf_tag.h
	kedr_trace_reader/kedr_trace_reader.h
	utils/template_parser.h
	utils/uuid.h
)

# List of include files to be installed.
set(KEDR_INCLUDE_FILES_INSTALL
	object_types.h
	kedr_mem/core_api.h
	kedr_mem/local_storage.h
	kedr_mem/block_info.h
	kedr_mem/functions.h
)

# Copy the headers from the src tree into the build tree
foreach(include_file ${KEDR_INCLUDE_FILES_SRC})
	set(include_file_src "${CMAKE_CURRENT_SOURCE_DIR}/kedr/${include_file}")
	get_filename_component(dest_dir 
		"${CMAKE_CURRENT_BINARY_DIR}/kedr/${include_file}" 
		PATH)
	file(MAKE_DIRECTORY "${dest_dir}")
	execute_process(COMMAND cp -p "${include_file_src}" "${dest_dir}"
		RESULT_VARIABLE copy_result)
	if(copy_result)
		message(FATAL_ERROR "Failed to copy header file ${include_file}.")
	endif(copy_result)
endforeach(include_file ${KEDR_INCLUDE_FILES_SRC})

if (NOT CMAKE_CROSSCOMPILING)
# Install rules for include files (files will be installed from binary tree!)
	foreach(include_file ${KEDR_INCLUDE_FILES_INSTALL})
		get_filename_component(dest_dir "${KEDR_INSTALL_PREFIX_INCLUDE}/${include_file}" PATH)
	    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr/${include_file}"
	            DESTINATION "${dest_dir}")
	endforeach(include_file ${KEDR_INCLUDE_FILES_INSTALL})

# Copy header files for examples.
#
# Because examples can use only installed header files, these files
# are copied into directory different from one for files used by
# components of the KEDR internally. These strategy allows to detect 
# missing install headers at the testing stage.
	foreach(include_file ${KEDR_INCLUDE_FILES_INSTALL})
	    set(include_file_src "${CMAKE_CURRENT_BINARY_DIR}/kedr/${include_file}")
		get_filename_component(dest_dir "${CMAKE_CURRENT_BINARY_DIR}/install/kedr/${include_file}" PATH)
		file(MAKE_DIRECTORY "${dest_dir}")
		execute_process(COMMAND cp -p "${include_file_src}" "${dest_dir}"
			RESULT_VARIABLE copy_result)
		if(copy_result)
			message(FATAL_ERROR "Failed to copy header file ${include_file} for examples.")
		endif(copy_result)
	endforeach(include_file ${KEDR_INCLUDE_FILES_SRC})
endif (NOT CMAKE_CROSSCOMPILING)
