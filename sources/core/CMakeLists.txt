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
#set(KEDR_MEM_CORE_SECTIONS_FILE "sections") 

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/core_impl.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/core_impl.h"
)

set(ARCH_DIR "arch/${KEDR_ARCH}")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/include/kedr/asm)

if (KEDR_64_BIT)
	set(THUNKS_SOURCE_FILE "thunks_64.S")
else ()
	set(THUNKS_SOURCE_FILE "thunks_32.S")
endif ()

kbuild_include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr")
set(ASM_OFFSETS_FILE "kedr_asm_offsets.h")

kbuild_add_module(${KMODULE_NAME}
# Sources:
	"module.c"
	"tid.c"
#	"sections.c"
	"module_ms_alloc.c"
	"i13n.c"
	"ifunc.c"
	"util.c"
	"ir.c"
	"insn_gen.c"
	"transform.c"
	"handlers.c"
	"resolve_ip.c"
	"annot_impl.c"
	"fh_impl.c"
	"${THUNKS_SOURCE_FILE}"

# Headers
	"core_impl.h"
	"tid.h"
#	"sections.h"
	"module_ms_alloc.h"
	"i13n.h"
	"ifunc.h"
	"util.h"
	"hooks.h"
	"ir.h"
	"insn_gen.h"
	"transform.h"
	"handlers.h"
	"thunks.h"
	"resolve_ip.h"
	"fh_impl.h"
	"target.h"

# Instruction decoder: sources and headers
	"${ARCH_DIR}/lib/inat.c"
	"${ARCH_DIR}/lib/insn.c"
	"${ARCH_DIR}/lib/inat-tables.h"
	"${ARCH_DIR}/include/kedr/asm/inat.h"
	"${ARCH_DIR}/include/kedr/asm/inat_types.h"
	"${ARCH_DIR}/include/kedr/asm/insn.h"

# Offsets of some fields in the structures needed by the functions
# written in assembly.
	"${ASM_OFFSETS_FILE}"

# The headers that can be used by other components (specify the headers
# here for CMake to create a dependency of the core module on these).
	"${TOP_INCLUDE_DIR}/object_types.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/local_storage.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/block_info.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/functions.h"
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

# Rules to generate the header file with the offsets of the needed fields
# in structs.
set(ASM_OFFSETS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asm_offsets")
set(ASM_OFFSETS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/asm_offsets")
set(ASM_OFFSETS_C_FILE_NAME "kedr_asm_offsets")

# The header files containing the structures we need the offsets for
# and the files these headers depend on.
set(HEADERS_FOR_OFFSETS 
	"${TOP_INCLUDE_DIR}/kedr_mem/local_storage.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/functions.h"
	"${CMAKE_SOURCE_DIR}/core/${ARCH_DIR}/include/kedr/asm/insn.h"
)

# Include directories and -I options
set(cflags_string)
if(kbuild_include_dirs)
	foreach(dir ${kbuild_include_dirs})
		set(cflags_string "${cflags_string} -I${dir}")
	endforeach(dir ${kmodule_include_dirs})
endif(kbuild_include_dirs)

configure_file (
	"${ASM_OFFSETS_SOURCE_DIR}/Kbuild.in"
	"${ASM_OFFSETS_BINARY_DIR}/Kbuild"
)

add_custom_command(OUTPUT "${ASM_OFFSETS_FILE}"
	COMMAND cp -p 
		"${ASM_OFFSETS_SOURCE_DIR}/${ASM_OFFSETS_C_FILE_NAME}.c"
		"${ASM_OFFSETS_BINARY_DIR}/${ASM_OFFSETS_C_FILE_NAME}.c"
	COMMAND $(MAKE) ARCH=${KEDR_ARCH} CROSS_COMPILE=${KEDR_CROSS_COMPILE} 
			-C ${KBUILD_BUILD_DIR} M="${ASM_OFFSETS_BINARY_DIR}"
	COMMAND cp -p 
		"${ASM_OFFSETS_BINARY_DIR}/${ASM_OFFSETS_FILE}"
		"${CMAKE_CURRENT_BINARY_DIR}/${ASM_OFFSETS_FILE}"
	DEPENDS 
		"${ASM_OFFSETS_SOURCE_DIR}/${ASM_OFFSETS_C_FILE_NAME}.c"
		${HEADERS_FOR_OFFSETS}
)
########################################################################

kedr_install_kmodule(${KMODULE_NAME})
kedr_install_symvers(${KMODULE_NAME})
########################################################################

# User-mode helpers
# [NB] These were used to get the addresses of the target's loaded sections
# only. KernelStrider no longer relies on that information.
#add_subdirectory(um_helpers)

# The files needed for the target modules using dynamic annotations
add_subdirectory(annotations)
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################

# Additional files to remove during 'make clean'
set(files_to_clean_extra
	"${ASM_OFFSETS_BINARY_DIR}/.tmp_versions"
	"${ASM_OFFSETS_BINARY_DIR}/Module.symvers"
	"${ASM_OFFSETS_BINARY_DIR}/modules.order"
	"${ASM_OFFSETS_BINARY_DIR}/Module.markers"
	"${ASM_OFFSETS_BINARY_DIR}/${ASM_OFFSETS_C_FILE_NAME}.c"
	"${ASM_OFFSETS_BINARY_DIR}/${ASM_OFFSETS_C_FILE_NAME}.s"
	"${ASM_OFFSETS_BINARY_DIR}/.${ASM_OFFSETS_C_FILE_NAME}.s.cmd"
	"${ASM_OFFSETS_BINARY_DIR}/.${ASM_OFFSETS_C_FILE_NAME}.s.d"
	"${ASM_OFFSETS_BINARY_DIR}/.${ASM_OFFSETS_C_FILE_NAME}.o.cmd"
	"${ASM_OFFSETS_BINARY_DIR}/.${ASM_OFFSETS_C_FILE_NAME}.o.d"
	"${ASM_OFFSETS_BINARY_DIR}/${ASM_OFFSETS_FILE}"
)
set_property (DIRECTORY APPEND PROPERTY 
    ADDITIONAL_MAKE_CLEAN_FILES ${files_to_clean_extra}
)
########################################################################
