if (CTF_OUTPUT_SUBSYSTEM)
	add_subdirectory(ctf_reader)
	add_subdirectory(kedr_trace_reader)
endif ()

add_subdirectory(simple_trace_recorder)
add_subdirectory(for_tsan)

if (KEDR_PYTHON_OK)
    # The convenience script to start/stop the kernel-mode part of the system.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/kedr.py.in"
        "${CMAKE_CURRENT_BINARY_DIR}/kedr.py"
        @ONLY)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr.py"
        DESTINATION ${KEDR_INSTALL_PREFIX_EXEC}
        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)
endif ()
