# Create the core kernel module.
set(KMODULE_NAME "kedr_mem_core")

# A prefix for the messages output to the system log
set(KEDR_MSG_PREFIX "[kedr_mem_core] ")

# Name of the directory to be created in debugfs for this component
# of the system.
set(KEDR_MEM_CORE_DEBUGFS_DIR "${KMODULE_NAME}")

# The file in ${KEDR_MEM_CORE_DEBUGFS_DIR} directory where the helper
# script should write the information about the sections of the target
# module.
set(KEDR_MEM_CORE_SECTIONS_FILE "sections") 

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/core_impl.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/core_impl.h"
)

set(ARCH_DIR "arch/${KEDR_ARCH}")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/include/kedr/asm)

kbuild_include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr")

kbuild_add_module(${KMODULE_NAME}
# Sources:
	"module.c"
	"tid.c"
	"sections.c"
	"module_ms_alloc.c"
	"i13n.c"

# Headers
	"core_impl.h"
	"tid.h"
	"sections.h"
	"module_ms_alloc.h"
	"i13n.h"

# Instruction decoder: sources and headers
    "${ARCH_DIR}/lib/inat.c"
    "${ARCH_DIR}/lib/insn.c"
    "${ARCH_DIR}/lib/inat-tables.h"
    "${ARCH_DIR}/include/kedr/asm/inat.h"
    "${ARCH_DIR}/include/kedr/asm/inat_types.h"
    "${ARCH_DIR}/include/kedr/asm/insn.h"

# The headers that can be used by other components (specify the headers
# here for CMake to create a dependency of the core module on these).
	"${TOP_INCLUDE_DIR}/object_types.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/local_storage.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/block_info.h"
)

# Generate the table with the information about the machine instructions
# in the form of the C language structures.
add_custom_command(
	OUTPUT 
		"${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib/inat-tables.h"
	COMMAND 
		LC_ALL=C awk -f 
		"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/tools/gen-insn-attr-x86.awk"
		"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/lib/x86-opcode-map.txt" > 
		"${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib/inat-tables.h"
	DEPENDS 
		"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/lib/x86-opcode-map.txt"
		"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/tools/gen-insn-attr-x86.awk"
)
########################################################################

kedr_install_kmodule(${KMODULE_NAME})
########################################################################

# User-mode helpers
add_subdirectory(um_helpers)
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
