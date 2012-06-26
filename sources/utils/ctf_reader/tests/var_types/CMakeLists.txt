set(executable_name "test_ctf_reader_var_types")

add_executable(${executable_name}
    "test.cpp")

target_link_libraries(${executable_name} ${ctf_reader_name})

kedr_test_add_target(${executable_name})

kedr_test_add("ctf_reader.var_types.01" "${executable_name}")