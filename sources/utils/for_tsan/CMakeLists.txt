kedr_load_install_prefixes()

install(PROGRAMS 
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_show_target_sections.sh"
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_show_target_symbols.sh"
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
########################################################################

add_subdirectory(symbolize)

option(WITH_REPORT_ADDR2LINE
	"Whether to build addr2line-style converter for Thread Sanitizer report"
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
			#message("Output of libelf example compilation:\n${libelf_compile_out}")
			
			message("It seems that libelf development library is absent on the system.")
			message("This library is needed for addr2line-style ThreadSanitizer report converter.")
			message("Please, install libelf development library")
			message("or disable building of the converter with -DWITH_REPORT_ADDR2LINE=OFF.")
			
			message(FATAL_ERROR "Libelf development library is not installed or cannot be used.")
		endif(NOT libelf_compile_result)
		
		set(libelf_headers "FOUND" CACHE INTERNAL "Whether libelf headers are installed")
		set(checking_message "${checking_message} - done")
	endif(libelf_headers)
	
	message(STATUS "${checking_message}")

	add_subdirectory(addr2line)
endif(WITH_REPORT_ADDR2LINE)
########################################################################
