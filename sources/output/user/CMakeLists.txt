set(trace_receiver_program_name "kedr_trace_receiver" CACHE INTERNAL
    "Name of the program which receive trace generated")

add_executable(${trace_receiver_program_name} "trace_receiver.cpp")

target_link_libraries(${trace_receiver_program_name} ${ctf_reader_name})

install(TARGETS "${trace_receiver_program_name}"
    DESTINATION "${KEDR_INSTALL_PREFIX_EXEC_AUX}")

# Used in config.h
set(KEDR_TRACE_RECEIVER_PATH
    "${KEDR_INSTALL_PREFIX_EXEC_AUX}/${trace_receiver_program_name}"
    CACHE INTERNAL "Full path to the installed trace receiver program")


set(trace_receiver_control_name "kedr_save_trace")


# Create template for usage of trace receiver control executable
set(trace_receiver_control_usage "trace_receiver_control_usage")

rule_copy_source("${trace_receiver_control_usage}")

# Create 'shipped' file with usage
add_custom_command(OUTPUT "${trace_receiver_control_usage}.o"
	COMMAND ld -o "${trace_receiver_control_usage}.o" "--format=binary"
		"${trace_receiver_control_usage}" "-r"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${trace_receiver_control_usage}")


add_executable(${trace_receiver_control_name}
    "trace_receiver_control.cpp"
    "${trace_receiver_control_usage}.o"
)

install(TARGETS "${trace_receiver_control_name}"
    DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}")

# For using in tests
set(trace_receiver_control_test_name "kedr_save_trace_test" CACHE
    INTERNAL "Name of the program which control trace receiver and used in tests.")
set(trace_receiver_control_test_dir "${CMAKE_CURRENT_BINARY_DIR}" CACHE
    INTERNAL "Directory of the program which control trace receiver and used in tests.")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/trace_receiver_control_test.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${trace_receiver_control_test_name}")

execute_process(COMMAND "chmod" "a+x"
    "${CMAKE_CURRENT_BINARY_DIR}/${trace_receiver_control_test_name}")
