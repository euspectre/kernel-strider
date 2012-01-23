# TODO

# Installation instructions for user-mode helpers.
# [NB] List the helper scripts in KEDR_UM_HELPERS.
set(KEDR_UM_HELPERS kedr_get_sections.sh)

kedr_load_install_prefixes()
install(PROGRAMS ${KEDR_UM_HELPERS} 
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
########################################################################
