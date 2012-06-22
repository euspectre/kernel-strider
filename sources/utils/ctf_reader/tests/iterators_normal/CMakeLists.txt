set(executable_name "test_ctf_reader_iterators_normal")

add_executable(${executable_name}
    "test.cpp")

target_link_libraries(${executable_name} ${ctf_reader_name})

kedr_test_add_target(${executable_name})

kedr_test_add("ctf_reader.iterators_normal.01" "${executable_name}"
    ${CMAKE_CURRENT_SOURCE_DIR})