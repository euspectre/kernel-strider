set(executable_name "kedr_trace_converter_tsan")

# Create 'shipped' file with usage
rule_copy_source("converter_usage")

add_custom_command(OUTPUT "converter_usage.o"
	COMMAND ld -o "converter_usage.o" "--format=binary"
		"converter_usage" "-r"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/converter_usage")


add_executable(${executable_name}
    "converter.cpp"
    "converter_usage.o")

target_link_libraries(${executable_name} elf)

target_link_libraries(${executable_name} ${kedr_trace_reader_name})

install(TARGETS "${executable_name}"
    DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}")
