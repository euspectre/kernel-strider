kedr_load_install_prefixes()

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_get_sections.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_get_sections.sh"
	@ONLY
)

# Installation instructions for user-mode helpers.
# Note that the relative paths are interpreted w.r.t. the current 
# source directory.
set(KEDR_UM_HELPERS 
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_get_sections.sh"
)

install(PROGRAMS ${KEDR_UM_HELPERS} 
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
########################################################################
