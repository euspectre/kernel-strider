set(kmodule_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(kmodule_test_sources_dir "${CMAKE_SOURCE_DIR}/cmake/kmodule_sources")

set(kmodule_function_map_file "")
if (CMAKE_CROSSCOMPILING)
	if (KEDR_SYSTEM_MAP_FILE)
		set (kmodule_function_map_file "${KEDR_SYSTEM_MAP_FILE}")
	else (KEDR_SYSTEM_MAP_FILE)
# KEDR_SYSTEM_MAP_FILE is not specified, construct the default path 
# to the symbol map file.
		set (kmodule_function_map_file 
	"${KEDR_ROOT_DIR}/boot/System.map-${KBUILD_VERSION_STRING}"
		)
	endif (KEDR_SYSTEM_MAP_FILE)
endif (CMAKE_CROSSCOMPILING)

# kmodule_try_compile(RESULT_VAR bindir srcfile
# 			[COMPILE_DEFINITIONS flags]
# 			[OUTPUT_VARIABLE var]
#			[COPY_FILE filename])

# Similar to try_module in simplified form, but compile srcfile as
# kernel module, instead of user space program.

function(kmodule_try_compile RESULT_VAR bindir srcfile)
	to_abs_path(src_abs_path "${srcfile}")
	# State for parse argument list
	set(state "None")
	foreach(arg ${ARGN})
		if(arg STREQUAL "COMPILE_DEFINITIONS")
			set(state "COMPILE_DEFINITIONS")
		elseif(arg STREQUAL "OUTPUT_VARIABLE")
			set(state "OUTPUT_VARIABLE")
		elseif(arg STREQUAL "COPY_FILE")
			set(state "COPY_FILE")
		elseif(state STREQUAL "COMPILE_DEFINITIONS")
			set(kmodule_cflags "${kmodule_cflags} ${arg}")
		elseif(state STREQUAL "OUTPUT_VARIABLE")
			set(output_variable "${arg}")
			set(state "None")
		elseif(state STREQUAL "COPY_FILE")
			set(copy_file_variable "${arg}")
			set(state "None")
		else(arg STREQUAL "COMPILE_DEFINITIONS")
			message(FATAL_ERROR 
				"Unexpected parameter passed to kmodule_try_compile: '${arg}'."
			)
		endif(arg STREQUAL "COMPILE_DEFINITIONS")
	endforeach(arg ${ARGN})
	set(cmake_params 
		"-DSRC_FILE:path=${src_abs_path}" 
		"-DKERNELDIR=${KBUILD_BUILD_DIR}"
		"-DKEDR_ARCH=${KEDR_ARCH}"
		"-DKEDR_CROSS_COMPILE=${KEDR_CROSS_COMPILE}"
	)
	if(DEFINED kmodule_cflags)
		list(APPEND cmake_params "-Dkmodule_flags=${kmodule_cflags}")
	endif(DEFINED kmodule_cflags)
	if(copy_file_variable)
		list(APPEND cmake_params "-DCOPY_FILE=${copy_file_variable}")
	endif(copy_file_variable)

	if(DEFINED output_variable)
		try_compile(result_tmp "${bindir}"
                "${kmodule_this_module_dir}/kmodule_files"
				"kmodule_try_compile_target"
                CMAKE_FLAGS ${cmake_params}
                OUTPUT_VARIABLE output_tmp)
		set("${output_variable}" "${output_tmp}" PARENT_SCOPE)
	else(DEFINED output_variable)
		try_compile(result_tmp "${bindir}"
                "${kmodule_this_module_dir}/kmodule_files"
				"kmodule_try_compile_target"
                CMAKE_FLAGS ${cmake_params})
	endif(DEFINED output_variable)
	set("${RESULT_VAR}" "${result_tmp}" PARENT_SCOPE)
endfunction(kmodule_try_compile RESULT_VAR bindir srcfile)

# List of unreliable functions, that is, the functions that may be
# be exported and mentioned in System.map but still cannot be used
# because no header provides their declarations.
set(unreliable_functions_list
    "__kmalloc_node"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmem_cache_alloc_node_trace"
)

# kmodule_is_function_exist(function_name RESULT_VAR)
# Verify, whether given function exist in the kernel space on the current system.
# RESULT_VAR is TRUE, if function_name exist in the kernel space, FALSE otherwise.
# RESULT_VAR is cached.

function(kmodule_is_function_exist function_name RESULT_VAR)
    set(kmodule_is_function_exist_message "Looking for ${function_name} in the kernel")

    if(DEFINED ${RESULT_VAR})
        set(kmodule_is_function_exist_message "${kmodule_is_function_exist_message} [cached]")
    else(DEFINED ${RESULT_VAR})
        execute_process(
            COMMAND sh "${kmodule_this_module_dir}/kmodule_files/scripts/lookup_kernel_function.sh"
				${function_name} ${kmodule_function_map_file}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            RESULT_VARIABLE kmodule_is_function_exist_result
            OUTPUT_QUIET)

		if (kmodule_is_function_exist_result EQUAL 0)
            list(FIND unreliable_functions_list ${function_name} unreliable_function_index)
            if(unreliable_function_index GREATER -1)
                # Additional verification for unreliable function
                kmodule_try_compile(kmodule_function_is_exist_reliable
                    "${CMAKE_BINARY_DIR}/check_unreliable_functions/${function_name}"
                    "${kmodule_test_sources_dir}/check_unreliable_functions/${function_name}.c"
                )
                if(NOT kmodule_function_is_exist_reliable)
                    set(kmodule_is_function_exist_result 1)
                endif(NOT kmodule_function_is_exist_reliable)
            endif(unreliable_function_index GREATER -1)
        endif(kmodule_is_function_exist_result EQUAL 0)

        if (kmodule_is_function_exist_result EQUAL 0)
            set(${RESULT_VAR} "TRUE" CACHE INTERNAL "Does ${function_name} exist in the kernel?")
        elseif(kmodule_is_function_exist_result EQUAL 1)
            set(${RESULT_VAR} "FALSE" CACHE INTERNAL "Does ${function_name} exist in the kernel?")
        else(kmodule_is_function_exist_result EQUAL 0)
            message(FATAL_ERROR 
"Cannot determine whether function '${function_name}' exists in the kernel"
			)
        endif(kmodule_is_function_exist_result EQUAL 0)
    endif(DEFINED ${RESULT_VAR})
    if (${RESULT_VAR})
        message(STATUS "${kmodule_is_function_exist_message} - found")
    else(${RESULT_VAR})
        message(STATUS "${kmodule_is_function_exist_message} - not found")
    endif(${RESULT_VAR})
endfunction(kmodule_is_function_exist function_name RESULT_VAR)

# Creates the list of functions that actually exist on the 
# current system.
#
# kmodule_configure_kernel_functions(output_list 
#	{[REQUIRED | OPTIONAL] {func | ONE_OF_LIST}} ...)
#
# ONE_OF_LIST := ONE_OF_BEGIN {func ...} ONE_OF_END
#
# There are 2 modes of function lookup: 
# OPTIONAL - if the function doesn't exist, it is silently ignored.
# REQUIRED - if the function doesn't exist, FATAL_ERROR message is printed.
#
# Initial mode is REQUIRED, and it can be changed at any time by REQUIRED 
# and OPTIONAL keywords.
#
# ONE_OF_BEGIN/ONE_OF_END determine a section for which no more than one 
# function among all listed there should exist. FATAL_ERROR message is 
# printed otherwise. When mode is REQUIRED, precisely one function must 
# exist.
# Inside this section other keywords must not be used (even another 
# ONE_OF_BEGIN).

function(kmodule_configure_kernel_functions output_list)
	set(kmodule_configure_kernel_functions_mode "REQUIRED")
	set(kmodule_configure_kernel_functions_one_of_section "FALSE")
	set(output_list_tmp)
    set(${output_list})
	foreach(arg ${ARGN})
		if(arg STREQUAL "REQUIRED" OR arg STREQUAL "OPTIONAL")
			if(kmodule_configure_kernel_functions_one_of_section)
				message(FATAL_ERROR 
"Inside ONE_OF_BEGIN/ONE_OF_END section, other keywords are not allowed."
				)
			endif(kmodule_configure_kernel_functions_one_of_section)
			set(kmodule_configure_kernel_functions_mode ${arg})
		elseif(arg STREQUAL "ONE_OF_BEGIN")
			if(kmodule_configure_kernel_functions_one_of_section)
				message(FATAL_ERROR "Nested ONE_OF_BEGIN/ONE_OF_END sections are not allowed.")
			endif(kmodule_configure_kernel_functions_one_of_section)
			set(kmodule_configure_kernel_functions_one_of_section "TRUE")
			set(kmodule_configure_kernel_functions_one_of_section_function)
		elseif(arg STREQUAL "ONE_OF_END")
			if(NOT kmodule_configure_kernel_functions_one_of_section)
				message(FATAL_ERROR "ONE_OF_END without ONE_OF_BEGIN is not allowed.")
			endif(NOT kmodule_configure_kernel_functions_one_of_section)
			if(kmodule_configure_kernel_functions_one_of_section_function)
				list(APPEND output_list_tmp ${kmodule_configure_kernel_functions_one_of_section_function})				
			else(kmodule_configure_kernel_functions_one_of_section_function)
				if(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
					message(FATAL_ERROR 
"None of the functions listed in ONE_OF section exist in the kernel but it is required."
					)
				endif(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
			endif(kmodule_configure_kernel_functions_one_of_section_function)
			set(kmodule_configure_kernel_functions_one_of_section "FALSE")
		else(arg STREQUAL "REQUIRED" OR arg STREQUAL "OPTIONAL")
			set(kmodule_func_varname _KMODULE_IS_${arg}_EXIST)
			kmodule_is_function_exist(${arg} ${kmodule_func_varname})
			if(kmodule_configure_kernel_functions_one_of_section)
				if(${kmodule_func_varname})
					if(kmodule_configure_kernel_functions_one_of_section_function)
						message(FATAL_ERROR "Two functions from ONE_OF sections exist in the kernel.")
					else(kmodule_configure_kernel_functions_one_of_section_function)
						set(kmodule_configure_kernel_functions_one_of_section_function ${arg})	
					endif(kmodule_configure_kernel_functions_one_of_section_function)
				endif(${kmodule_func_varname})
			else(kmodule_configure_kernel_functions_one_of_section)
				if(${kmodule_func_varname})
					list(APPEND output_list_tmp ${arg})
				else(${kmodule_func_varname})
					if(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
						message(FATAL_ERROR "Function ${arg} is not found in the kernel but it is required.")
					endif(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
				endif(${kmodule_func_varname})
			endif(kmodule_configure_kernel_functions_one_of_section)
		endif(arg STREQUAL "REQUIRED" OR arg STREQUAL "OPTIONAL")
	endforeach(arg ${ARGN})
	if(kmodule_configure_kernel_functions_one_of_section)
		message(FATAL_ERROR "Found ONE_OF_BEGIN without ONE_OF_END")
	endif(kmodule_configure_kernel_functions_one_of_section)
    set(${output_list} ${output_list_tmp} PARENT_SCOPE)
endfunction(kmodule_configure_kernel_functions output_list)

############################################################################
# Utility macros to check for particular features. If the particular feature
# is supported, the macros will set the corresponding variable to TRUE, 
# otherwise - to FALSE (the name of variable is mentioned in the comments 
# for the macro). 
############################################################################

# Check if the system has everything necessary to build at least simple
# kernel modules. 
# The macro sets variable 'MODULE_BUILD_SUPPORTED'.
macro(check_module_build)
	set(check_module_build_message 
		"Checking if kernel modules can be built on this system"
	)
	message(STATUS "${check_module_build_message}")
	if (DEFINED MODULE_BUILD_SUPPORTED)
		set(check_module_build_message 
"${check_module_build_message} [cached] - ${MODULE_BUILD_SUPPORTED}"
		)
	else (DEFINED MODULE_BUILD_SUPPORTED)
		kmodule_try_compile(module_build_supported_impl 
			"${CMAKE_BINARY_DIR}/check_module_build"
			"${kmodule_test_sources_dir}/check_module_build/module.c"
		)
		if (module_build_supported_impl)
			set(MODULE_BUILD_SUPPORTED "yes" CACHE INTERNAL
				"Can kernel modules be built on this system?"
			)
		else (module_build_supported_impl)
			set(MODULE_BUILD_SUPPORTED "no")
			message(FATAL_ERROR 
"There are problems with building kernel modules on this system. "
"Please check that the appropriate kernel headers and build tools "
"are installed."
			)
		endif (module_build_supported_impl)
				
		set(check_module_build_message 
"${check_module_build_message} - ${MODULE_BUILD_SUPPORTED}"
		)
	endif (DEFINED MODULE_BUILD_SUPPORTED)
	message(STATUS "${check_module_build_message}")
endmacro(check_module_build)

# Check if the version of the kernel is acceptable
# The macro sets variable 'KERNEL_VERSION_OK'.
# It also sets 'KERNEL_VERSION', which is the version of the kernel in the
# form "major.minor.micro".
macro(check_kernel_version kversion_major kversion_minor kversion_micro)
	set(check_kernel_version_string 
"${kversion_major}.${kversion_minor}.${kversion_micro}"
	)
	set(check_kernel_version_message 
"Checking if the kernel version is ${check_kernel_version_string} or newer"
	)
	message(STATUS "${check_kernel_version_message}")

	string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
		KERNEL_VERSION
		"${KBUILD_VERSION_STRING}"
	)
	
	if (DEFINED KERNEL_VERSION_OK)
		set(check_kernel_version_message 
"${check_kernel_version_message} [cached] - ${KERNEL_VERSION_OK}"
		)
	else (DEFINED KERNEL_VERSION_OK)
		if (KERNEL_VERSION VERSION_LESS check_kernel_version_string)
			set(KERNEL_VERSION_OK "no")
			message(FATAL_ERROR 
"Kernel version is ${KERNEL_VERSION} but ${check_kernel_version_string} or newer is required."
			)
		else ()
			set(KERNEL_VERSION_OK "yes" CACHE INTERNAL
				"Is kernel version high enough?"
			)
		endif ()
				
		set(check_kernel_version_message 
"${check_kernel_version_message} - ${KERNEL_VERSION_OK}"
		)
	endif (DEFINED KERNEL_VERSION_OK)
	message(STATUS "${check_kernel_version_message}")
endmacro(check_kernel_version kversion_major kversion_minor kversion_micro)

# Check if reliable stack trace information can be obtained. 
# This is the case, for example, if the kernel is compiled with support
# for frame pointers and/or stack unwind on.
# The macro sets variable 'STACK_TRACE_RELIABLE'.
macro(check_stack_trace)
	set(check_stack_trace_message 
		"Checking if stack trace information is reliable"
	)
	message(STATUS "${check_stack_trace_message}")
	if (DEFINED STACK_TRACE_RELIABLE)
		set(check_stack_trace_message 
"${check_stack_trace_message} [cached] - ${STACK_TRACE_RELIABLE}"
		)
	else (DEFINED STACK_TRACE_RELIABLE)
		kmodule_try_compile(stack_trace_reliable_impl 
			"${CMAKE_BINARY_DIR}/check_stack_trace"
			"${kmodule_test_sources_dir}/check_stack_trace/module.c"
		)
		if (stack_trace_reliable_impl)
			set(STACK_TRACE_RELIABLE "yes" CACHE INTERNAL
				"Are stack traces reliable on this system?"
			)
		else (stack_trace_reliable_impl)
			set(STACK_TRACE_RELIABLE "no" CACHE INTERNAL
				"Are stack traces reliable on this system?"
			)
		endif (stack_trace_reliable_impl)
				
		set(check_stack_trace_message 
"${check_stack_trace_message} - ${STACK_TRACE_RELIABLE}"
		)
	endif (DEFINED STACK_TRACE_RELIABLE)
	message(STATUS "${check_stack_trace_message}")

	if (NOT STACK_TRACE_RELIABLE)
		message ("\n[WARNING]\n"
		"It looks like reliable stack traces cannot be obtained on this system.\n"
		"The output of the tools provided by this package can be less detailed.\n"
		"If this is not acceptable, you could rebuild the kernel with\n"
		"CONFIG_FRAME_POINTER or CONFIG_STACK_UNWIND (if available) set to \"y\"\n"
		"and then reconfigure and rebuild the package.\n")
	endif (NOT STACK_TRACE_RELIABLE)
endmacro(check_stack_trace)

# Check whether ring buffer is implemented by the kernel.
# Set cache variable RING_BUFFER_IMPLEMENTED according to this checking.
function(check_ring_buffer)
	set(check_ring_buffer_message 
		"Checking if ring buffer is implemented in the kernel"
	)
	message(STATUS "${check_ring_buffer_message}")
	if (DEFINED RING_BUFFER_IMPLEMENTED)
		set(check_ring_buffer_message 
"${check_ring_buffer_message} [cached] - ${RING_BUFFER_IMPLEMENTED}"
		)
	else (DEFINED RING_BUFFER_IMPLEMENTED)
		kmodule_try_compile(ring_buffer_implemented_impl 
			"${CMAKE_BINARY_DIR}/check_ring_buffer"
			"${kmodule_test_sources_dir}/check_ring_buffer/module.c"
		)
		if (ring_buffer_implemented_impl)
			set(RING_BUFFER_IMPLEMENTED "yes" CACHE INTERNAL
				"Whether ring buffer is implemented in the kernel"
			)
		else (ring_buffer_implemented_impl)
			set(RING_BUFFER_IMPLEMENTED "no" CACHE INTERNAL
				"Whether ring buffer is implemented in the kernel"
			)
		endif (ring_buffer_implemented_impl)
				
		set(check_ring_buffer_message 
"${check_ring_buffer_message} - ${RING_BUFFER_IMPLEMENTED}"
		)
	endif (DEFINED RING_BUFFER_IMPLEMENTED)
	message(STATUS "${check_ring_buffer_message}")
	
	if (NOT RING_BUFFER_IMPLEMENTED)
		message(FATAL_ERROR
			"\n[WARNING]\n Ring buffer functionality is not supported by "
			"the kernel (CONFIG_RING_BUFFER is not set in the kernel "
			"config file).\n"
			"Ring buffer support is needed for the components collecting "
			"the data obtained by the core.\n")
	endif (NOT RING_BUFFER_IMPLEMENTED)

endfunction(check_ring_buffer)

# Check which memory allocator is used by the kernel.
# Set KERNEL_MEMORY_ALLOCATOR to 'slab', 'slub', 'slob' or 'other'.
#
# Some functions in that allocators may have same names, but different signatures.
function(check_allocator)
	set(check_allocator_message 
		"Checking which memory allocator is used by the kernel"
	)
		message(STATUS "${check_allocator_message}")
	if (DEFINED KERNEL_MEMORY_ALLOCATOR)
		set(check_allocator_message 
"${check_allocator_message} [cached] - ${KERNEL_MEMORY_ALLOCATOR}"
		)
	else (DEFINED KERNEL_MEMORY_ALLOCATOR)
		kmodule_try_compile(is_allocator_slab 
			"${CMAKE_BINARY_DIR}/check_allocator_slab"
			"${kmodule_test_sources_dir}/check_allocator/module.c"
			COMPILE_DEFINITIONS "-DIS_ALLOCATOR_SLAB"
		)
		if (is_allocator_slab)
			set(allocator "slab")
		else (is_allocator_slab)
			kmodule_try_compile(is_allocator_slub 
				"${CMAKE_BINARY_DIR}/check_allocator_slub"
				"${kmodule_test_sources_dir}/check_allocator/module.c"
				COMPILE_DEFINITIONS "-DIS_ALLOCATOR_SLUB"
			)
			if (is_allocator_slub)
				set(allocator "slub")
			else (is_allocator_slub)
				kmodule_try_compile(is_allocator_slob 
					"${CMAKE_BINARY_DIR}/check_allocator_slob"
					"${kmodule_test_sources_dir}/check_allocator/module.c"
					COMPILE_DEFINITIONS "-DIS_ALLOCATOR_SLOB"
				)
				if (is_allocator_slob)
					set(allocator "slob")
				else (is_allocator_slub)
					set(allocator "other")
				endif (is_allocator_slob)
			endif (is_allocator_slub)
		endif (is_allocator_slab)
		set(KERNEL_MEMORY_ALLOCATOR "${allocator}" CACHE INTERNAL
			"Memory allocator which is used by the kernel"
		)
				
		set(check_allocator_message 
"${check_allocator_message} - ${KERNEL_MEMORY_ALLOCATOR}"
		)
	endif (DEFINED KERNEL_MEMORY_ALLOCATOR)
	message(STATUS "${check_allocator_message}")

endfunction(check_allocator)

# Check if 'kfree_rcu' is available in the kernel (it is likely to be 
# a macro or an inline). If it is available, we should handle it as 
# 'free' in LeakCheck. As KEDR cannot normally intercept kfree_rcu()
# itself, it needs to intercept call_rcu/call_rcu_sched and check their
# arguments.
# The macro sets variable 'HAVE_KFREE_RCU'.
macro(check_kfree_rcu)
	set(check_kfree_rcu_message 
		"Checking if kfree_rcu() is available"
	)
	message(STATUS "${check_kfree_rcu_message}")
	if (DEFINED HAVE_KFREE_RCU)
		set(check_kfree_rcu_message 
"${check_kfree_rcu_message} [cached] - ${HAVE_KFREE_RCU}"
		)
	else (DEFINED HAVE_KFREE_RCU)
		kmodule_try_compile(have_kfree_rcu_impl 
			"${CMAKE_BINARY_DIR}/check_kfree_rcu"
			"${kmodule_test_sources_dir}/check_kfree_rcu/module.c"
		)
		if (have_kfree_rcu_impl)
			set(HAVE_KFREE_RCU "yes" CACHE INTERNAL
				"Is kfree_rcu() available?"
			)
		else (have_kfree_rcu_impl)
			set(HAVE_KFREE_RCU "no" CACHE INTERNAL
				"Is kfree_rcu() available?"
			)
		endif (have_kfree_rcu_impl)
				
		set(check_kfree_rcu_message 
"${check_kfree_rcu_message} - ${HAVE_KFREE_RCU}"
		)
	endif (DEFINED HAVE_KFREE_RCU)
	message(STATUS "${check_kfree_rcu_message}")
endmacro(check_kfree_rcu)

# Check if the required kernel parameters are set in the kernel 
# configuration.
macro(check_kernel_config)
	set(check_kernel_config_message 
		"Checking the basic configuration of the kernel"
	)
	message(STATUS "${check_kernel_config_message}")
	if (DEFINED KERNEL_CONFIG_OK)
		message(STATUS "${check_kernel_config_message} [cached] - ok")
	else (DEFINED KERNEL_CONFIG_OK)
		kmodule_try_compile(kernel_config_impl 
			"${CMAKE_BINARY_DIR}/check_kernel_config"
			"${kmodule_test_sources_dir}/check_kernel_config/module.c"
		)
		if (kernel_config_impl)
			set(KERNEL_CONFIG_OK "yes" CACHE INTERNAL
				"Are the necessary basic kernel configuration parameters set?"
			)
			message(STATUS "${check_kernel_config_message} - ok")
		else (kernel_config_impl)
			message(FATAL_ERROR 
				"Some of the required configuration parameters of the kernel "
				"are not set. Please check the configuration file for the "
				"kernel.\n"
				"The following parameters should be set:\n"
				"\tCONFIG_X86_32 or CONFIG_X86_64 (system architecture)\n"
				"\tCONFIG_MODULES (loadable module support)\n"
				"\tCONFIG_MODULE_UNLOAD (module unloading support)\n"
				"\tCONFIG_SYSFS (sysfs support)\n"
				"\tCONFIG_DEBUG_FS (debugfs support)\n"
				"\tCONFIG_KALLSYMS (loading of kernel symbols in the kernel image)\n"
			)
		endif (kernel_config_impl)
	endif (DEFINED KERNEL_CONFIG_OK)
endmacro(check_kernel_config)

# Check whether the virtual memory split configuration is acceptable.
function(check_vm_split)
	set(check_vm_split_message
		"Checking if the virtual memory split configuration is acceptable"
	)
	message(STATUS "${check_vm_split_message}")
	if (DEFINED VM_SPLIT_OK)
		set(check_vm_split_message
"${check_vm_split_message} [cached] - ${VM_SPLIT_OK}"
		)
	else (DEFINED VM_SPLIT_OK)
		kmodule_try_compile(vm_split_ok_impl
			"${CMAKE_BINARY_DIR}/check_vm_split"
			"${kmodule_test_sources_dir}/check_vm_split/module.c"
		)
		if (vm_split_ok_impl)
			set(VM_SPLIT_OK "yes" CACHE INTERNAL
				"Is current VM split configuration supported?"
			)
		else (vm_split_ok_impl)
			set(VM_SPLIT_OK "no" CACHE INTERNAL
				"Is current VM split configuration supported?"
			)
		endif (vm_split_ok_impl)

		set(check_vm_split_message
"${check_vm_split_message} - ${VM_SPLIT_OK}"
		)
	endif (DEFINED VM_SPLIT_OK)
	message(STATUS "${check_vm_split_message}")

	if (NOT VM_SPLIT_OK)
		message(FATAL_ERROR
			"\n[WARNING]\n"
			"The kernels with CONFIG_VMSPLIT_2G_OPT=y or CONFIG_VMSPLIT_1G=y "
			"are not supported by the thread handling subsystem of the core.\n"
		)
	endif (NOT VM_SPLIT_OK)

endfunction(check_vm_split)

# Check if __alloc_workqueue_key() accepts a variable argument list and
# if so, set KEDR_ALLOC_WQ_KEY_VARARG.
function(check_alloc_wq_key)
	set(check_alloc_wq_key_message 
		"Checking if __alloc_workqueue_key() accepts variable argument list"
	)
	message(STATUS "${check_alloc_wq_key_message}")
	if (DEFINED ALLOC_WQ_KEY_VARARG)
		set(check_alloc_wq_key_message 
"${check_alloc_wq_key_message} [cached] - ${ALLOC_WQ_KEY_VARARG}"
		)
	else ()
		kmodule_try_compile(alloc_wq_key_vararg_impl 
			"${CMAKE_BINARY_DIR}/check_alloc_wq_key"
			"${kmodule_test_sources_dir}/check_alloc_workqueue_key/module.c"
		)
		if (alloc_wq_key_vararg_impl)
			set(ALLOC_WQ_KEY_VARARG "yes" CACHE INTERNAL
				"Does __alloc_workqueue_key() accept variable argument list?"
			)
		else ()
			set(ALLOC_WQ_KEY_VARARG "no" CACHE INTERNAL
				"Does __alloc_workqueue_key() accept variable argument list?"
			)
		endif (alloc_wq_key_vararg_impl)
				
		set(check_alloc_wq_key_message 
"${check_alloc_wq_key_message} - ${ALLOC_WQ_KEY_VARARG}"
		)
	endif (DEFINED ALLOC_WQ_KEY_VARARG)
	message(STATUS "${check_alloc_wq_key_message}")
	
	if (ALLOC_WQ_KEY_VARARG)
		set(KEDR_ALLOC_WQ_KEY_VARARG 1 CACHE INTERNAL 
			"Preprocessor symbol for ALLOC_WQ_KEY_VARARG."
		)
	endif ()
endfunction(check_alloc_wq_key)
############################################################################

# Check if hlist_for_each_entry*() macros accept only 'type *pos' argument
# rather than both 'type *tpos' and 'hlist_node *pos' as the loop cursors.
# The macro sets variable 'HLIST_FOR_EACH_ENTRY_POS_ONLY'.
macro(check_hlist_for_each_entry)
	set(check_hlist_for_each_entry_message
"Checking the signatures of hlist_for_each_entry*() macros"
	)
	message(STATUS "${check_hlist_for_each_entry_message}")
	if (DEFINED HLIST_FOR_EACH_ENTRY_POS_ONLY)
		set(check_hlist_for_each_entry_message
"${check_hlist_for_each_entry_message} [cached] - done"
		)
	else ()
		kmodule_try_compile(pos_only_impl
			"${CMAKE_BINARY_DIR}/check_hlist_for_each_entry"
			"${kmodule_test_sources_dir}/check_hlist_for_each_entry/module.c"
		)
		if (pos_only_impl)
			set(HLIST_FOR_EACH_ENTRY_POS_ONLY "yes" CACHE INTERNAL
	"Do hlist_for_each_entry*() macros have only 'type *pos' to use as a loop cursor?"
			)
		else ()
			set(HLIST_FOR_EACH_ENTRY_POS_ONLY "no" CACHE INTERNAL
	"Do hlist_for_each_entry*() macros have only 'type *pos' to use as a loop cursor?"
			)
		endif (pos_only_impl)

		set(check_hlist_for_each_entry_message
			"${check_hlist_for_each_entry_message} - done"
		)
	endif (DEFINED HLIST_FOR_EACH_ENTRY_POS_ONLY)
	message(STATUS "${check_hlist_for_each_entry_message}")
endmacro(check_hlist_for_each_entry)
############################################################################

# Check if 'random32' is available in the kernel.
# The macro sets variable 'KEDR_HAVE_RANDOM32'.
macro(check_random32)
	set(check_random32_message
		"Checking if random32() is available"
	)
	message(STATUS "${check_random32_message}")
	if (DEFINED KEDR_HAVE_RANDOM32)
		set(check_random32_message
"${check_random32_message} [cached] - ${KEDR_HAVE_RANDOM32}"
		)
	else (DEFINED KEDR_HAVE_RANDOM32)
		kmodule_try_compile(have_random32_impl
			"${CMAKE_BINARY_DIR}/check_random32"
			"${kmodule_test_sources_dir}/check_random32/module.c"
		)
		if (have_random32_impl)
			set(KEDR_HAVE_RANDOM32 "yes" CACHE INTERNAL
				"Is random32() available?"
			)
		else (have_random32_impl)
			set(KEDR_HAVE_RANDOM32 "no" CACHE INTERNAL
				"Is random32() available?"
			)
		endif (have_random32_impl)

		set(check_random32_message
"${check_random32_message} - ${KEDR_HAVE_RANDOM32}"
		)
	endif (DEFINED KEDR_HAVE_RANDOM32)
	message(STATUS "${check_random32_message}")
endmacro(check_random32)
############################################################################

# Check if 'request_firmware_nowait' accepts 7 arguments.
# In 2.6.32, accepts 6 arguments, in 2.6.33 and newer kernels - 7 arguments.
# The last argument is the callback function in each case.
# 
# The macro sets variable 'REQUEST_FW_HAS_7_ARGS'.
macro(check_request_fw)
	set(check_request_fw_message
		"Checking the signature of request_firmware_nowait()"
	)
	message(STATUS "${check_request_fw_message}")
	if (DEFINED REQUEST_FW_HAS_7_ARGS) 
		set(check_request_fw_message
"${check_request_fw_message} [cached] - done"
		)
	else (DEFINED REQUEST_FW_HAS_7_ARGS)
		kmodule_try_compile(request_fw_has_7_args_impl
			"${CMAKE_BINARY_DIR}/check_request_fw"
			"${kmodule_test_sources_dir}/check_request_fw/module.c"
		)

		if (request_fw_has_7_args_impl)
			set(REQUEST_FW_HAS_7_ARGS "yes" CACHE INTERNAL
				"Does request_firmware_nowait() have 7 arguments?"
			)
		else (request_fw_has_7_args_impl)
			set(REQUEST_FW_HAS_7_ARGS "no" CACHE INTERNAL
				"Does request_firmware_nowait() have 7 arguments?"
			)
		endif (request_fw_has_7_args_impl)

		set(check_request_fw_message
"${check_request_fw_message} - done"
		)
	endif (DEFINED REQUEST_FW_HAS_7_ARGS)
	message(STATUS "${check_request_fw_message}")
endmacro(check_request_fw)
############################################################################

# Check if net_device_ops::ndo_fdb_add has 'struct net_device *' as its 
# 2nd or 3rd argument. Sets KEDR_NDO_FDB_ADD_DEV2 or KEDR_NDO_FDB_ADD_DEV3
# (or none) accordingly.
#
# Note that we add '-Werror' to the compile definitions because GCC only
# warns about incompatible pointer types if the type of the callback does 
# not match but does not issue an error by default.
function(check_ndo_fdb_add)
	set(check_ndo_fdb_add_message 
		"Checking the signature of net_device_ops::ndo_fdb_add()"
	)
	message(STATUS "${check_ndo_fdb_add_message}")
	if (DEFINED NDO_FDB_ADD_SIG)
		set(check_ndo_fdb_add_message 
"${check_ndo_fdb_add_message} [cached] - done"
		)
	else ()
		kmodule_try_compile(ndo_fdb_add_impl 
			"${CMAKE_BINARY_DIR}/check_ndo_fdb_add"
			"${kmodule_test_sources_dir}/check_ndo_fdb_add/module.c"
			COMPILE_DEFINITIONS "-Werror -DIS_NDO_FDB_ADD_DEV2"
		)
		if (ndo_fdb_add_impl)
			set(NDO_FDB_ADD_SIG "yes" CACHE INTERNAL
				"Is the signature of net_device_ops::ndo_fdb_add() known?"
			)
			set(KEDR_NDO_FDB_ADD_DEV2 1 CACHE INTERNAL 
				"net_device_ops::ndo_fdb_add(): dev is the 2nd arg.")
		else ()
			kmodule_try_compile(ndo_fdb_add_impl3 
				"${CMAKE_BINARY_DIR}/check_ndo_fdb_add"
				"${kmodule_test_sources_dir}/check_ndo_fdb_add/module.c"
				COMPILE_DEFINITIONS "-Werror -DIS_NDO_FDB_ADD_DEV3"
			)
			if (ndo_fdb_add_impl3)
				set(NDO_FDB_ADD_SIG "yes" CACHE INTERNAL
					"Is the signature of net_device_ops::ndo_fdb_add() known?"
				)
				set(KEDR_NDO_FDB_ADD_DEV3 1 CACHE INTERNAL 
					"net_device_ops::ndo_fdb_add(): dev is the 3rd arg.")
			else ()
				message(FATAL_ERROR 
					"Unknown signature of net_device_ops::ndo_fdb_add()")
			endif (ndo_fdb_add_impl3)
		endif (ndo_fdb_add_impl)
				
		set(check_ndo_fdb_add_message 
"${check_ndo_fdb_add_message} - done"
		)
	endif (DEFINED NDO_FDB_ADD_SIG)
	message(STATUS "${check_ndo_fdb_add_message}")
endfunction(check_ndo_fdb_add)

# A similar checker but for ndo_fdb_del.
function(check_ndo_fdb_del)
	set(check_ndo_fdb_del_message 
		"Checking the signature of net_device_ops::ndo_fdb_del()"
	)
	message(STATUS "${check_ndo_fdb_del_message}")
	if (DEFINED NDO_FDB_DEL_SIG)
		set(check_ndo_fdb_del_message 
"${check_ndo_fdb_del_message} [cached] - done"
		)
	else ()
		kmodule_try_compile(ndo_fdb_del_impl 
			"${CMAKE_BINARY_DIR}/check_ndo_fdb_del"
			"${kmodule_test_sources_dir}/check_ndo_fdb_del/module.c"
			COMPILE_DEFINITIONS "-Werror -DIS_NDO_FDB_DEL_DEV2"
		)
		if (ndo_fdb_del_impl)
			set(NDO_FDB_DEL_SIG "yes" CACHE INTERNAL
				"Is the signature of net_device_ops::ndo_fdb_del() known?"
			)
			set(KEDR_NDO_FDB_DEL_DEV2 1 CACHE INTERNAL 
				"net_device_ops::ndo_fdb_del(): dev is the 2nd arg.")
		else ()
			kmodule_try_compile(ndo_fdb_del_impl3 
				"${CMAKE_BINARY_DIR}/check_ndo_fdb_del"
				"${kmodule_test_sources_dir}/check_ndo_fdb_del/module.c"
				COMPILE_DEFINITIONS "-Werror -DIS_NDO_FDB_DEL_DEV3"
			)
			if (ndo_fdb_del_impl3)
				set(NDO_FDB_DEL_SIG "yes" CACHE INTERNAL
					"Is the signature of net_device_ops::ndo_fdb_del() known?"
				)
				set(KEDR_NDO_FDB_DEL_DEV3 1 CACHE INTERNAL 
					"net_device_ops::ndo_fdb_del(): dev is the 3rd arg.")
			else ()
				kmodule_try_compile(ndo_fdb_del_impl 
					"${CMAKE_BINARY_DIR}/check_ndo_fdb_del"
					"${kmodule_test_sources_dir}/check_ndo_fdb_del/module.c"
					COMPILE_DEFINITIONS "-Werror -DIS_NDO_FDB_DEL_DEV2_NOCONST"
				)
				if (ndo_fdb_del_impl)
					set(NDO_FDB_DEL_SIG "yes" CACHE INTERNAL
						"Is the signature of net_device_ops::ndo_fdb_del() known?"
					)
					set(KEDR_NDO_FDB_DEL_DEV2 1 CACHE INTERNAL 
						"net_device_ops::ndo_fdb_del(): dev is the 2nd arg.")
				else()
					message(FATAL_ERROR 
						"Unknown signature of net_device_ops::ndo_fdb_del()")
				endif (ndo_fdb_del_impl)
			endif (ndo_fdb_del_impl3)
		endif (ndo_fdb_del_impl)
				
		set(check_ndo_fdb_del_message 
"${check_ndo_fdb_del_message} - done"
		)
	endif (DEFINED NDO_FDB_DEL_SIG)
	message(STATUS "${check_ndo_fdb_del_message}")
endfunction(check_ndo_fdb_del)
############################################################################

# Check if the kernel is built with KEDR annotations enabled.
function(check_kedr_annotations)
	set(check_kedr_annotations_message
		"Checking if the kernel is built with KEDR annotations enabled"
	)
	message(STATUS "${check_kedr_annotations_message}")
	if (DEFINED KEDR_ANNOTATIONS_ENABLED)
		set(check_kedr_annotations_message
"${check_kedr_annotations_message} [cached] - ${KEDR_ANNOTATIONS_ENABLED}"
		)
	else (DEFINED KEDR_ANNOTATIONS_ENABLED)
		kmodule_try_compile(kedr_annotations_ok_impl
			"${CMAKE_BINARY_DIR}/check_kedr_annotations"
			"${kmodule_test_sources_dir}/check_kedr_annotations/module.c"
		)
		if (kedr_annotations_ok_impl)
			set(KEDR_ANNOTATIONS_ENABLED "yes" CACHE INTERNAL
				"Is the kernel built with KEDR annotations enabled?"
			)
		else (kedr_annotations_ok_impl)
			set(KEDR_ANNOTATIONS_ENABLED "no" CACHE INTERNAL
				"Is the kernel built with KEDR annotations enabled?"
			)
		endif (kedr_annotations_ok_impl)

		set(check_kedr_annotations_message
"${check_kedr_annotations_message} - ${KEDR_ANNOTATIONS_ENABLED}"
		)
	endif (DEFINED KEDR_ANNOTATIONS_ENABLED)
	message(STATUS "${check_kedr_annotations_message}")
endfunction(check_kedr_annotations)
############################################################################
