set(engine_name "tsan_report_processor")
set(resolver_name "resolver_addr2line")
set(converter_name "tsan_report_addr2line")

add_executable(${engine_name} tsan_report_processor.c)
add_executable(${resolver_name} resolver_addr2line.cpp)

target_link_libraries(${resolver_name} elf)

install(TARGETS "${engine_name}" ${resolver_name}
    DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}")

install(FILES ${converter_name}
	DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
	PERMISSIONS OWNER_WRITE
				OWNER_READ GROUP_READ WORLD_READ
				OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)