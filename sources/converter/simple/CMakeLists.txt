set(executable_name "kedr_trace_converter_simple")

add_executable(${executable_name}
    "converter.cpp")

target_link_libraries(${executable_name} ${kedr_trace_reader_name})
