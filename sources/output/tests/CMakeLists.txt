set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates")

set(trace_sender_module_name "test_trace_sender")
set(trace_sender_module_dir "${CMAKE_CURRENT_BINARY_DIR}/trace_sender")

add_subdirectory(core_stub)
add_subdirectory(test_messages)
add_subdirectory(test_events)
add_subdirectory(trace_sender)

