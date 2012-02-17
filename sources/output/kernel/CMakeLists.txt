# Create module for sending trace.

# Name of the module.
#
# Used in unit testing.
# May be moved upper for integration system testing(as whole).
set(trace_sender_module_name "kedr_trace_sender")
set(trace_sender_module_dir "${CMAKE_CURRENT_BINARY_DIR}")

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


# Determine target machine parameters
set(checking_message "Determine target machine parameters")
# Helper function for determine machine type from kernel module
function(_get_machine_type RESULT_VAR elf_file)
	execute_process(
		COMMAND readelf -h "${elf_file}"
		COMMAND sed -n -e "/Machine:/ {s/[[:blank:]]\\{0,\\}Machine:[[:blank:]]\\{0,\\}//; p}"
		RESULT_VARIABLE machine_type_result
		OUTPUT_VARIABLE machine_type_output
		ERROR_VARIABLE machine_type_error
		OUTPUT_STRIP_TRAILING_WHITESPACE)
		if(NOT machine_type_result EQUAL 0)
			message(FATAL_ERROR
				"Error occure while determine target machine type:\n${machine_type_error}"
			)
		endif(NOT machine_type_result EQUAL 0)
	set(${RESULT_VAR} "${machine_type_output}" PARENT_SCOPE)
endfunction(_get_machine_type RESULT_VAR elf_file)
set(module_temp "${CMAKE_CURRENT_BINARY_DIR}/module_temp.ko")

message(STATUS "${checking_message}")
if(DEFINED TARGET_POINTER_SIZE AND DEFINED TARGET_MACHINE_TYPE)
	set(checking_message "${checking_message} [cached]")
else(DEFINED TARGET_POINTER_SIZE AND DEFINED TARGET_MACHINE_TYPE)
	kmodule_try_compile(is_size_of_pointer_32 # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/size_of_pointer_is_32" # bindir
		"determine_pointer_size.c" # srcfile
		COMPILE_DEFINITIONS "-DEXPECTED_POINTER_SIZE_32"
		OUTPUT_VARIABLE out_32
		COPY_FILE "${module_temp}")

	if(is_size_of_pointer_32)
		set(TARGET_POINTER_SIZE "32" CACHE INTERNAL
			"Size of pointer (in bits)")
	else(is_size_of_pointer_32)
		kmodule_try_compile(is_size_of_pointer_64 # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/size_of_pointer_is_64" # bindir
			"determine_pointer_size.c" # srcfile
			COMPILE_DEFINITIONS "-DEXPECTED_POINTER_SIZE_64"
			OUTPUT_VARIABLE out_64
			COPY_FILE "${module_temp}")
		if(is_size_of_pointer_64)
			set(TARGET_POINTER_SIZE "64" CACHE INTERNAL
			"Size of pointer (in bits)")
		else(is_size_of_pointer_64)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unexpected size of pointer.")
			message("Output of test compilation for size=32:")
			message("${out_32}")
			message("Output of test compilation for size=64:")
			message("${out_64}")
			message(FATAL_ERROR "")
		endif(is_size_of_pointer_64)
	endif(is_size_of_pointer_32)
	
	_get_machine_type(machine_type "${module_temp}")
	set(TARGET_MACHINE_TYPE "${machine_type}" CACHE INTERNAL
		"Target machine type")
endif(DEFINED TARGET_POINTER_SIZE AND DEFINED TARGET_MACHINE_TYPE)
set(checking_message "${checking_message} - ${TARGET_MACHINE_TYPE}, ${TARGET_POINTER_SIZE}-bit")
message(STATUS "${checking_message}")

# Create format string for CTF trace metadata
set(meta_format_file_rel "meta_format")
set(meta_format_shipped_file "${CMAKE_CURRENT_BINARY_DIR}/meta_format.o_shipped")

set(CTF_MACHINE_TYPE "${TARGET_MACHINE_TYPE}")
set(CTF_POINTER_BITS_DECLARATION "${TARGET_POINTER_SIZE}")
set(CTF_POINTER_BITS "${TARGET_POINTER_SIZE}")
set(CTF_UUID_STR "%.4x-%.2x-%.2x-%.2x-%.2x%.4x")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../ctf_meta.in"
	"${CMAKE_CURRENT_BINARY_DIR}/${meta_format_file_rel}"
	@ONLY)

# Create 'shipped' file with CTF trace metadata format
add_custom_command(OUTPUT "${meta_format_shipped_file}"
	COMMAND ld -o "${meta_format_shipped_file}" "--format=binary"
		"${meta_format_file_rel}" "-r"
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${meta_format_file_rel}")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/event_collector/Module.symvers")
kbuild_add_dependencies("kedr_event_collector")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME}
	"trace_server.c"
	"trace_sender.c"
	"trace_packets.c"
	"net_message.c"
	"uuid_generator.c"

	"${meta_format_shipped_file}"

	"trace_sender.h"
	"trace_packets.h"
	"net_message.h"
	"uuid_generator.h"
)

########################################################################

kedr_install_kmodule(${KMODULE_NAME})
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
