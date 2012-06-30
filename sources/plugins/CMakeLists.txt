
if(KEDR_COI_INSTALL_DIR)
    # Plugins which depends on KEDR COI
    
    # Common infrastructure for use KEDR COI.
    set(interceptors_dir ${KEDR_COI_INSTALL_DIR}/share/kedr-coi/interceptors)
    set(interceptor_templates_dir ${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates)

    # Create interceptor files from datafile
    function(rule_interceptor_source source_file data_file)
        add_custom_command(OUTPUT ${source_file}
            COMMAND ${KEDR_GEN_TOOL}
                ${interceptor_templates_dir}/kedr_coi_interceptor.c
                ${data_file} > ${source_file}
            DEPENDS ${data_file})
    endfunction(rule_interceptor_source source_file data_file)
    
    function(rule_interceptor_header header_file data_file)
        add_custom_command(OUTPUT ${header_file}
            COMMAND ${KEDR_GEN_TOOL}
                ${interceptor_templates_dir}/kedr_coi_interceptor.h
                ${data_file} > ${header_file}
            DEPENDS ${data_file})
    endfunction(rule_interceptor_header header_file data_file)

    function(rule_factory_interceptor_source source_file data_file)
        add_custom_command(OUTPUT ${source_file}
            COMMAND ${KEDR_GEN_TOOL}
                ${interceptor_templates_dir}/kedr_coi_factory_interceptor.c
                ${data_file} > ${source_file}
            DEPENDS ${data_file})
    endfunction(rule_factory_interceptor_source source_file data_file)

    function(rule_factory_interceptor_header header_file data_file)
        add_custom_command(OUTPUT ${header_file}
            COMMAND ${KEDR_GEN_TOOL}
                ${interceptor_templates_dir}/kedr_coi_factory_interceptor.h
                ${data_file} > ${header_file}
            DEPENDS ${data_file})
    endfunction(rule_factory_interceptor_header header_file data_file)

    # Plugins themselves
    add_subdirectory(cdev)
endif(KEDR_COI_INSTALL_DIR)
