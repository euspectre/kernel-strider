kedr_load_install_prefixes()

install(PROGRAMS 
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_show_target_sections.sh"
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_show_target_symbols.sh"
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
########################################################################

add_subdirectory(symbolize)
add_subdirectory(addr2line)
add_subdirectory(process_trace)
########################################################################
