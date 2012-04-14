set(executable_name "test_ctf_meta_simple")

add_executable(${executable_name}
    "test.c")

target_link_libraries(${executable_name} ${ctf_meta_constructor_name})

kedr_test_add_target(${executable_name})

kedr_test_add("ctf_reader.ctf_meta.simple.01" "${executable_name}")