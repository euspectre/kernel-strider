# List of include files that do not need configuring and should be 
# copied to the build tree as is.
# [NB] When you add a header to the "include" subtree of the source 
# tree, do not forget to add it to the list below.
set(KEDR_INCLUDE_FILES_SRC
	object_types.h
	kedr_mem/core_api.h
	kedr_mem/local_storage.h
	kedr_mem/block_info.h
	event_collector/event_collector.h
	event_collector/event_handler.h
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
