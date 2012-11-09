# Create module for sending trace.

# Name of the module.
#
# Used in unit testing.
# May be moved upper for integration system testing(as whole).
set(KMODULE_NAME ${trace_sender_module_name})

# Determine how to create module parameters with callbacks
set(checking_message "Checking the ways to create module parameters with callbacks")
message(STATUS "${checking_message}")
if((DEFINED MODULE_PARAM_CREATE_USE_OPS_STRUCT) OR (DEFINED MODULE_PARAM_CREATE_USE_OPS))
	set(checking_message "${checking_message} [cached] - done")
else((DEFINED MODULE_PARAM_CREATE_USE_OPS_STRUCT) OR (DEFINED MODULE_PARAM_CREATE_USE_OPS))
	kmodule_try_compile(is_module_param_create_use_ops_struct # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/module_param_create_use_ops_struct" # bindir
		"module_param_create_use_ops_struct.c" # srcfile
		OUTPUT_VARIABLE out_ops_struct)

	if(is_module_param_create_use_ops_struct)
		set(checking_message "${checking_message} - done")
		set(MODULE_PARAM_CREATE_USE_OPS_STRUCT "TRUE" CACHE INTERNAL
			"To create module parameters with callbacks, one needs to use struct ops")
	else(is_module_param_create_use_ops_struct)
		kmodule_try_compile(is_module_param_create_use_ops # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/module_param_create_use_ops" # bindir
			"module_param_create_use_ops.c" # srcfile
			OUTPUT_VARIABLE out_ops)
		if(is_module_param_create_use_ops)
			set(checking_message "${checking_message} - done")
			set(MODULE_PARAM_CREATE_USE_OPS "TRUE" CACHE INTERNAL
			"To create module parameters with callbacks, one needs to use ops")
		else(is_module_param_create_use_ops)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unknown way to create module param with callbacks.")
			message("Output of test compilation for operations structure:")
			message("${out_ops_srtuct}")
			message("Output of test compilation for operations:")
			message("${out_ops}")
			message(FATAL_ERROR "")
		endif(is_module_param_create_use_ops)
	endif(is_module_param_create_use_ops_struct)
endif((DEFINED MODULE_PARAM_CREATE_USE_OPS_STRUCT) OR (DEFINED MODULE_PARAM_CREATE_USE_OPS))
message(STATUS "${checking_message}")

set(checking_message "Checking ring_buffer_consume signature")
message(STATUS "${checking_message}")
if((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
	set(checking_message "${checking_message} [cached] - done")
else((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
	kmodule_try_compile(is_ring_buffer_consume_has_4_args # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_consume_4_args" # bindir
		"ring_buffer_consume_signature.c" # srcfile
		COMPILE_DEFINITIONS "-DRING_BUFFER_CONSUME_HAS_4_ARGS"
		OUTPUT_VARIABLE out_4)

	if(is_ring_buffer_consume_has_4_args)
		set(checking_message "${checking_message} - done")
		set(RING_BUFFER_CONSUME_HAS_4_ARGS "TRUE" CACHE INTERNAL
			"Whether ring_buffer_consume accept 4 arguments")
	else(is_ring_buffer_consume_has_4_args)
		kmodule_try_compile(is_ring_buffer_consume_has_3_args # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_consume_3_args" # bindir
			"ring_buffer_consume_signature.c" # srcfile
			COMPILE_DEFINITIONS "-DRING_BUFFER_CONSUME_HAS_3_ARGS"
			OUTPUT_VARIABLE out_3)
		if(is_ring_buffer_consume_has_3_args)
			set(checking_message "${checking_message} - done")
			set(RING_BUFFER_CONSUME_HAS_3_ARGS "TRUE" CACHE INTERNAL
			"Whether ring_buffer_consume accept 3 arguments")
		else(is_ring_buffer_consume_has_3_args)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unknown signature of ring_buffer_consume.")
			message("Output of test compilation for 3 arguments:")
			message("${out_3}")
			message("Output of test compilation for 4 arguments:")
			message("${out_4}")
			message(FATAL_ERROR "")
		endif(is_ring_buffer_consume_has_3_args)
	endif(is_ring_buffer_consume_has_4_args)
endif((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
message(STATUS "${checking_message}")



# Create template for CTF trace metadata
set(meta_template_file "ctf_meta_template")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/${meta_template_file}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../${meta_template_file}")

# Create 'shipped' file with CTF trace metadata format
add_custom_command(OUTPUT "${meta_template_shipped_file}"
	COMMAND ld -o "${meta_template_shipped_file}" "--format=binary"
		"${meta_template_file}" "-r"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${meta_template_file}")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr_mem_core")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME}
	"event_collector.c"
	"trace_server.c"
	"trace_sender.c"
	"trace_packets.c"
	"net_message.c"
	"uuid_generator.c"

	"${meta_template_shipped_file}"

	"trace_sender.h"
	"trace_packets.h"
	"net_message.h"
	"uuid_generator.h"
	"trace_definition.h"
)

########################################################################
kedr_install_kmodule(${KMODULE_NAME})

