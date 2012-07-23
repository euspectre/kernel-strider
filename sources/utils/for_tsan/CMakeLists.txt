kedr_load_install_prefixes()

install(PROGRAMS 
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_show_target_sections.sh"
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_show_target_symbols.sh"
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
########################################################################

add_subdirectory(symbolize)

option(WITH_REPORT_ADDR2LINE
	"Whether to build addr2line-style converter of Thread Sanitizer reports"
	ON)
if(WITH_REPORT_ADDR2LINE)

	# Libelf is required for addr2line-style Thread Sanitizer report convertion.
	set(checking_message "Checking for libelf headers")
	message(STATUS "${checking_message}")

	if(libelf_headers)
		set(checking_message "${checking_message} [cached] - done")
	else(libelf_headers)
		try_compile(libelf_compile_result # Result variable
			"${CMAKE_CURRENT_BINARY_DIR}/libelf_check" # Binary dir
			"${CMAKE_CURRENT_SOURCE_DIR}/libelf_check.c" # Source file
			CMAKE_FLAGS "-DLINK_LIBRARIES:string=elf"
			OUTPUT_VARIABLE libelf_compile_out)
		if(NOT libelf_compile_result)
			message("It seems that the development files for libelf library are not")
			message("present on this system. The files are needed to build the")
			message("addr2line-style converter of ThreadSanitizer reports.")
			message("Please, install the development files for libelf library or use")
			message("-DWITH_REPORT_ADDR2LINE=OFF to disable building of the converter.")
			
			message(FATAL_ERROR 
				"Development files for libelf are not installed or cannot be used.")
		endif(NOT libelf_compile_result)
		
		set(libelf_headers "FOUND" CACHE INTERNAL "Whether libelf headers are installed")
		set(checking_message "${checking_message} - done")
	endif(libelf_headers)
	
	message(STATUS "${checking_message}")

	add_subdirectory(addr2line)
endif(WITH_REPORT_ADDR2LINE)
########################################################################
