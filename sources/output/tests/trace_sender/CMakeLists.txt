set(KMODULE_NAME "${trace_sender_module_name}")
set(ORIG_BIN_DIR "${CMAKE_BINARY_DIR}/output/kernel")
set(ORIG_SRC_DIR "${CMAKE_SOURCE_DIR}/output/kernel")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/output/tests/core_stub/Module.symvers")
kbuild_add_dependencies("test_core_stub")

# needed for the sources to be prepared in the binary tree
kbuild_add_dependencies("kedr_trace_sender") 

kbuild_include_directories("${ORIG_SRC_DIR}")

set(src_files
	"event_collector.c"
	"trace_server.c"
	"trace_sender.c"
	"trace_packets.c"
	"net_message.c"
	"uuid_generator.c"

	"${meta_template_shipped_file}"
)

set(header_files
	"${ORIG_SRC_DIR}/trace_sender.h"
	"${ORIG_SRC_DIR}/trace_packets.h"
	"${ORIG_SRC_DIR}/net_message.h"
	"${ORIG_SRC_DIR}/uuid_generator.h"
	"${ORIG_SRC_DIR}/trace_definition.h"
)

foreach(s_file ${src_files})
	rule_copy_file(
		"${CMAKE_CURRENT_BINARY_DIR}/${s_file}"
		"${ORIG_BIN_DIR}/${s_file}")
endforeach()

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME}
	${src_files}
	${header_files}
)

kedr_test_add_target(${KMODULE_NAME})
