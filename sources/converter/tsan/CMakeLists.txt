set(executable_name "kedr_trace_converter_tsan")

add_executable(${executable_name}
    "converter.cpp")

target_link_libraries(${executable_name} ${kedr_trace_reader_name})

install(TARGETS "${executable_name}"
    DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}")
