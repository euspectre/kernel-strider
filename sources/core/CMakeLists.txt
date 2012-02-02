# Create the core kernel module.
set(KMODULE_NAME "kedr_mem_core")

set(ARCH_DIR "arch/${KEDR_ARCH}")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/include/kedr/asm)

kbuild_include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

kbuild_add_module(${KMODULE_NAME}
# Sources:
	"module.c"
	"tid.c"

# Headers
	"core_impl.h"
	"tid.h"

# Instruction decoder: sources and headers
    "${ARCH_DIR}/lib/inat.c"
    "${ARCH_DIR}/lib/insn.c"
    "${ARCH_DIR}/lib/inat-tables.h"
    "${ARCH_DIR}/include/kedr/asm/inat.h"
    "${ARCH_DIR}/include/kedr/asm/inat_types.h"
    "${ARCH_DIR}/include/kedr/asm/insn.h"
)

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib/inat-tables.h"
	COMMAND LC_ALL=C awk -f "${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/tools/gen-insn-attr-x86.awk"
		"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/lib/x86-opcode-map.txt" >
		"${CMAKE_CURRENT_BINARY_DIR}/${ARCH_DIR}/lib/inat-tables.h"
	DEPENDS 
		"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/lib/x86-opcode-map.txt"
		"${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIR}/tools/gen-insn-attr-x86.awk"
	)

########################################################################

kedr_install_kmodule(${KMODULE_NAME})
########################################################################

# Tests
kedr_test_add_subdirectory(tests)
########################################################################
