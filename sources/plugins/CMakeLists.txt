#Rule for generate files with kedr_gen
function(rule_generate_file target_file data_file template)
    add_custom_command(OUTPUT ${target_file}
        COMMAND ${KEDR_GEN_TOOL} ${template} ${data_file} > ${target_file}
        DEPENDS ${data_file})
endfunction(rule_generate_file target_file data_file template)

set(model_templates_dir ${CMAKE_CURRENT_SOURCE_DIR}/templates)

if(KEDR_COI_INSTALL_DIR)
    # Plugins which depends on KEDR COI
    
    # Common infrastructure for use KEDR COI.
    set(interceptors_dir ${KEDR_COI_INSTALL_DIR}/share/kedr-coi/interceptors)
    set(interceptor_templates_dir ${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates)

    # Plugins themselves
    add_subdirectory(cdev)
    add_subdirectory(file_system)
endif(KEDR_COI_INSTALL_DIR)
