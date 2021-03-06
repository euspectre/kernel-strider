cmake_minimum_required(VERSION 2.6)

enable_language(C)
enable_language(CXX)
#######################################################################

# Prohibit a common type of an in-source build.
# Note that building in a subdirectory in the source tree is still allowed 
# as it can be convenient.
string (COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" in_source)
if (in_source)
    message (FATAL_ERROR 
"It is not allowed to build the project in its source directory. "
"Please delete CMakeCache.txt and CMakeFiles from ${CMAKE_SOURCE_DIR} "
"if they are there and then use some other directory to build KEDR."
)
endif () 
#######################################################################

# Names and versions
set(KEDR_PACKAGE_NAME "kedr")
set(KEDR_KS_PACKAGE_NAME "KernelStrider")

set(KEDR_KS_VERSION_MAJOR 0)
set(KEDR_KS_VERSION_MINOR 4)
set(KEDR_KS_VERSION_MICRO 0)
set(KEDR_KS_VERSION_SUFFIX "-devel" CACHE STRING
#set(KEDR_KS_VERSION_SUFFIX "" CACHE STRING
	"Version suffix, a string that should be appended to the version"
)
set(KEDR_KS_PACKAGE_VERSION 
"${KEDR_KS_VERSION_MAJOR}.${KEDR_KS_VERSION_MINOR}${KEDR_KS_VERSION_SUFFIX}"
)
#######################################################################

# ARCH and CROSS_COMPILE variables that can be used when building 
# kernel modules. It is convenient to have them as environment variables
# because Makefiles will be able to use them directly.
if (NOT KEDR_ARCH)
	set (KEDR_ARCH "x86")
endif ()

set (ENV{KEDR_ARCH} "${KEDR_ARCH}")
set (ENV{KEDR_CROSS_COMPILE} "${KEDR_CROSS_COMPILE}")

if (CMAKE_SIZEOF_VOID_P EQUAL 8) 
	set (KEDR_64_BIT "yes")
else ()
	set (KEDR_64_BIT "no")
endif()
#######################################################################

# [NB] Sparse is usually not installed by default. If you are going to
# use it, make sure it is installed.
option(KEDR_USE_SPARSE 
	"Use Sparse analysis tool when building kernel modules." 
	OFF
)
#######################################################################

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
find_package(Kbuild)

# Taken from CMake 3.8.12 because 3.6.4-3.8.0+ do not provide convenient
# means to get the version of Python, which is needed here.
find_package(PythonInterp)
if (PYTHON_VERSION_STRING VERSION_LESS "2.7.0")
	set(KEDR_PYTHON_OK "FALSE")
	message(WARNING "\nPython version 2.7 or newer is needed for "
		"some of the tools in this package (e.g. kedr.py helper). "
		"Such tools will not be installed.")
else ()
	set(KEDR_PYTHON_OK "TRUE")
endif ()

include(cmake_useful)
include(kbuild_system)
include(kmodule)

include(path_prefixes)
#######################################################################

# Initialize test-related stuff
kedr_test_init()
#######################################################################

# kedr_install_library(library_name)
function(kedr_install_library library_name)
	install(TARGETS ${library_name} LIBRARY
			DESTINATION ${KEDR_INSTALL_PREFIX_LIB})
endfunction(kedr_install_library library_name)
# kedr_install_headers(install_subdir header_file [..])
function(kedr_install_headers install_subdir)
	install(FILES ${header_file} ${ARGN}
			DESTINATION ${KEDR_INSTALL_PREFIX_INCLUDE}/${install_subdir})
endfunction(kedr_install_headers install_subdir)
# kedr_install_kmodule(kmodule_name)
function(kedr_install_kmodule kmodule_name)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${kmodule_name}.ko"
			DESTINATION "${KEDR_INSTALL_PREFIX_KMODULE}")
endfunction(kedr_install_kmodule kmodule_name)
# kedr_install_symvers(kmodule_name)
function(kedr_install_symvers kmodule_name)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Module.symvers"
			DESTINATION "${KEDR_INSTALL_PREFIX_KSYMVERS}"
			RENAME "${kmodule_name}.symvers")
endfunction(kedr_install_symvers kmodule_name)
#######################################################################

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

add_custom_target (uninstall_files
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

add_custom_target (uninstall)

# During uninstall process, the files should be removed first, then 
# the directories.
# 'uninstall_dirs' target is defined in cmake/modules/path_prefixes.cmake.
add_dependencies (uninstall_dirs uninstall_files)
add_dependencies (uninstall uninstall_dirs)
#######################################################################

include_directories("${CMAKE_BINARY_DIR}/include")
kbuild_include_directories("${CMAKE_BINARY_DIR}/include")

# For the configuration file (config.h)
include_directories("${CMAKE_BINARY_DIR}")
kbuild_include_directories("${CMAKE_BINARY_DIR}")
#######################################################################

# Make "Release" the default build type
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo."
      FORCE)
endif ()
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    kbuild_add_definitions(
        "-g -DKEDR_DEBUG"
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    kbuild_add_definitions(
        "-g"
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
else(CMAKE_BUILD_TYPE STREQUAL "Debug")
	message(FATAL_ERROR "Unknown type of build: ${CMAKE_BUILD_TYPE}.")
endif()

# Flags to compiler when build user-space programs
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -Wall -Wextra")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -g -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")
#######################################################################

# The path where the user-mode helper scripts are to be installed
set(KEDR_UM_HELPER_PATH "${KEDR_INSTALL_PREFIX_EXEC}")
#######################################################################

# Perform basic checks

# Check if we use kernel version 3.2 or newer.
# This macro also set KERNEL_VERSION.
check_kernel_version(3 2 0)

# Check if kernel modules can be built on this system.
check_module_build()

# Check kernel configuration.
check_kernel_config()

# Check if the ring buffer functionality is available.
check_ring_buffer()

# Check if the VM split configuration is acceptable.
check_vm_split()

# Check if the kernel is built with KEDR annotations enabled.
check_kedr_annotations()

# Find out which memory allocator is used by the kernel. It is needed
# because, for example, the order of arguments to kmem_cache_alloc*_trace()
# may be different depending on the allocator.
check_allocator()

if (${KERNEL_MEMORY_ALLOCATOR} STREQUAL "slab" AND
	KERNEL_VERSION VERSION_LESS "3.7.0")
	set (KMCA_TRACE_SIZE_FIRST 1)
else ()
	set (KMCA_TRACE_KMC_FIRST 1)
endif ()

# readelf can also be necessary when building and testing the package
message (STATUS "Checking for readelf")
find_program (READELF readelf)
if (READELF)
	message (STATUS "Checking for readelf - ${READELF}")
else (READELF)
	message (FATAL_ERROR "readelf is not found")
endif (READELF)

# addr2line is needed to properly resolve the source lines in the 
# reports.
message (STATUS "Checking for addr2line")
find_program (ADDR2LINE addr2line)
if (ADDR2LINE)
	message (STATUS "Checking for addr2line - ${ADDR2LINE}")
else (ADDR2LINE)
	message (FATAL_ERROR "addr2line is not found")
endif (ADDR2LINE)

# Check if the development files for libelf and libdw are available.
check_libelf_devel()
check_libdw_devel()

# Check if stack trace information is reliable on this system.
# STACK_TRACE_RELIABLE will hold the result.
# This only checks relevant configuration parameters of kernel.
#check_stack_trace()

# Check the signatures of hlist_for_each_entry*() macros, they changed 
# in the kernel 3.9.
check_hlist_for_each_entry()

# Check if random32() is available.
check_random32()

# Find ThreadSanitizer offline if the user did not specify the path to it.
# ${TSAN_APP} is the path to the application if found, a false value
# otherwise.
if (NOT TSAN_APP)
	find_tsan(TSAN_APP)
endif (NOT TSAN_APP)
#######################################################################

# kedr_gen (this time - for building KEDR itself)
if (KEDR_GEN)
# If cross-compiling for a different architecture, kedr_gen tool 
# will probably not build here, so it should be built separately 
# in advance in this case. The path to kedr_gen should be passed here 
# in KEDR_GEN variable.

# ${KEDR_GEN_TOOL} is the path to "kedr_gen". The path can be used to 
# execute "kedr_gen" tool during the build of payload modules, etc.
	set (KEDR_GEN_TOOL ${KEDR_GEN})
	
	# Check if the tool exists at the path specified.
	execute_process (
	    COMMAND ${KEDR_GEN} 
	    RESULT_VARIABLE kedr_gen_exec_result
		OUTPUT_QUIET
	)
	if (NOT kedr_gen_exec_result EQUAL 0)
	    message (FATAL_ERROR 
	"${KEDR_GEN} does not exist or cannot be executed."
	    )
	endif ()
	
	message (STATUS "\"kedr_gen\" tool: ${KEDR_GEN}")

else (KEDR_GEN)
# kedr_gen is built here explicitly and installed to a temporary location.
# This is only necessary to build KEDR itself.
# This should be done before the first add_subdirectory() command.

# The temporary install directory must be persistent and it must not be 
# KEDR_INSTALL_PREFIX_TEMP because it has nothing to do with the 
# installation of KEDR: it is used when KEDR is being built only.

	message (STATUS "Creating \"kedr_gen\"")
	set (KEDR_GEN_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/kedr_gen.install")
	set (KEDR_GEN_TEMP_BUILD "${CMAKE_CURRENT_BINARY_DIR}/kedr_gen.build")

# The temporary build directory should be removed during 'make clean'
# Note that the temporary install directory containing 'kedr_gen' must 
# survive as kedr_gen will be necessary if the user wants to rebuild KEDR 
# after 'make clean' without reconfiguring.
	set_property (DIRECTORY APPEND PROPERTY 
	    ADDITIONAL_MAKE_CLEAN_FILES kedr_gen.build
	)

# Remove the old temporary build and install directories first and 
# (re)create them.

# [NB] Note that if several commands are specified in execute_process() 
# call, it is equivalent to executing a pipeline in the shell:
#   command1 | command2 | ...
# Generally, there are no restrictions on the order in which the commands 
# will be executed and on whether some of the commands will execute 
# concurrently. Therefore, here is the rule of thumb:
# [!!!] Specify 2 or more commands in a single execute_process() statement
# if and only if the pipeline of the commands is what you want or if it is 
# not important whether they will actually run simultaneously. 
# Otherwise, you should use separate execute_process() statements.
#
# For example, using execute_process() like this is definitely looking 
# for trouble:
#   execute_process (
#       COMMAND rm -rf "some_dir" 
#       COMMAND mkdir -p "some_dir" 
#   )
# The order in which these commands will do their work is unspecified. 
# It may happen that 'mkdir' will finish before 'rm' will begin removing
# "some_dir". This is probably not what you expect here.

#Do not destroy the previously installed generator
#execute_process (
#    COMMAND rm -rf "${KEDR_GEN_TEMP_BUILD}" 
#    COMMAND rm -rf "${KEDR_GEN_INSTALL_PREFIX}" 
#)

# [NB] After the command is executed, the variable specified 
# in RESULT_VARIABLE may contain either numeric return code or a string 
# describing the error.
	execute_process (
	    COMMAND mkdir -p "${KEDR_GEN_TEMP_BUILD}" 
	    RESULT_VARIABLE kedr_gen_result
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message (FATAL_ERROR 
	"Failed to create directory ${KEDR_GEN_TEMP_BUILD}"
	    )
	endif ()

	execute_process (
	    COMMAND mkdir -p "${KEDR_GEN_INSTALL_PREFIX}" 
	    RESULT_VARIABLE kedr_gen_result
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message (FATAL_ERROR 
	"Failed to create directory ${KEDR_GEN_TEMP_BUILD}"
	    )
	endif ()

	# Configure kedr_gen
	execute_process (
	    COMMAND ${CMAKE_COMMAND} 
	        -DCMAKE_INSTALL_PREFIX=${KEDR_GEN_INSTALL_PREFIX}
	        -DCMAKE_BUILD_TYPE=Release
	        -DKEDR_GEN_INSTALL_PREFIX=${KEDR_GEN_INSTALL_PREFIX}
	        "${CMAKE_CURRENT_SOURCE_DIR}/tools/kedr_gen/src"
	    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
	    RESULT_VARIABLE kedr_gen_result
	    OUTPUT_VARIABLE kedr_gen_output
	    ERROR_VARIABLE  kedr_gen_output
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message ("Failed to configure \"kedr_gen\".")
	    message ("CMake output:\n${kedr_gen_output}\n")
	    message ("CMake result:\n${kedr_gen_result}\n")
	    message (FATAL_ERROR "Unable to build \"kedr_gen\", aborting.")
	endif ()

	# Build kedr_gen
	execute_process (
	    COMMAND make
	    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
	    RESULT_VARIABLE kedr_gen_result
	    OUTPUT_VARIABLE kedr_gen_output
	    ERROR_VARIABLE  kedr_gen_output
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message ("Failed to build \"kedr_gen\".")
	    message ("Make output:\n${kedr_gen_output}\n")
	    message ("Make result:\n${kedr_gen_result}\n")
	    message (FATAL_ERROR "Unable to build \"kedr_gen\", aborting.")
	endif ()

	# Install kedr_gen
	execute_process (
	    COMMAND make install
	    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
	    RESULT_VARIABLE kedr_gen_result
	    OUTPUT_VARIABLE kedr_gen_output
	    ERROR_VARIABLE  kedr_gen_output
	)
	if (NOT kedr_gen_result EQUAL 0)
	    message ("Failed to install \"kedr_gen\" to ${KEDR_GEN_INSTALL_PREFIX}.")
	    message ("Make output:\n${kedr_gen_output}\n")
	    message ("Make result:\n${kedr_gen_result}\n")
	    message (FATAL_ERROR "Unable to install \"kedr_gen\", aborting.")
	endif ()

# ${KEDR_GEN_TOOL} is the path to "kedr_gen". The path can be used to call
# "kedr_gen" tool during the build of payload modules, etc.
	set (KEDR_GEN_TOOL ${KEDR_GEN_INSTALL_PREFIX}/kedr_gen)
endif (KEDR_GEN)

message (STATUS "Creating \"kedr_gen\" - done")
#######################################################################

# The CTF-based output subsystem still needs work to be usable. It is now
# disabled by default.
option (CTF_OUTPUT_SUBSYSTEM 
	"Build the components to output and process trace in CTF-like format." 
	OFF
)
#######################################################################

add_subdirectory(core)
add_subdirectory(functions)

# Should be before 'output'.
add_subdirectory(utils)

if (CTF_OUTPUT_SUBSYSTEM)
	add_subdirectory(output)
	add_subdirectory(converter)
endif (CTF_OUTPUT_SUBSYSTEM)

add_subdirectory(include)

add_subdirectory(tools)

add_subdirectory(examples)
#######################################################################

# "Global" tests 
kedr_test_add_subdirectory(tests)
#######################################################################

configure_file("${CMAKE_SOURCE_DIR}/config.h.in" 
	"${CMAKE_BINARY_DIR}/config.h")
message(STATUS "Creating config.h - done")
#######################################################################

message(STATUS 
	"Configured ${KEDR_KS_PACKAGE_NAME} version ${KEDR_KS_PACKAGE_VERSION}")
#######################################################################
