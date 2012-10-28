# Create the kernel module to handle the function calls relevant for
# data race detection. The functions handlers here can be used by different
# kernel subsystems, hence "common" in the name.
set(KMODULE_NAME "kedr_fh_drd_common")

set(COMBINED_DATA_FILE "${CMAKE_CURRENT_BINARY_DIR}/func.data")
set(MAIN_C_FILE "module.c")

# The following function groups must always be processed.
set(FUNCTION_GROUPS 
	"alloc" 
	"realloc" 
	"free" 
	"lock" 
	"unlock"
	"uaccess"
	"dup"
	"strings"
	"thread"
)

set(HANDLER_C_FILES)
set(HANDLER_DATA_FILES)
set(GROUP_TARGETS)
foreach(group ${FUNCTION_GROUPS})
	list(APPEND GROUP_TARGETS "func_drd_${group}")
	list(APPEND HANDLER_DATA_FILES 
		"${CMAKE_CURRENT_BINARY_DIR}/groups/${group}/handlers.data")
	list(APPEND HANDLER_C_FILES
		"${CMAKE_CURRENT_BINARY_DIR}/handlers_${group}.c")
	rule_copy_file(
		"${CMAKE_CURRENT_BINARY_DIR}/handlers_${group}.c"
		"${CMAKE_CURRENT_BINARY_DIR}/groups/${group}/handlers_${group}.c"
	)
endforeach(group ${FUNCTION_GROUPS})

# The rule to create a combined .data file.
add_custom_command(OUTPUT ${COMBINED_DATA_FILE}
	COMMAND cat ${HANDLER_DATA_FILES} > ${COMBINED_DATA_FILE}
)
add_custom_target(${KMODULE_NAME}_data_file
	DEPENDS ${COMBINED_DATA_FILE}
)
add_dependencies(${KMODULE_NAME}_data_file ${GROUP_TARGETS})

# The rule to create the main source file of the module.
add_custom_command(OUTPUT ${MAIN_C_FILE}
	COMMAND ${KEDR_GEN_TOOL} 
		"${CMAKE_CURRENT_SOURCE_DIR}/templates"
		${COMBINED_DATA_FILE} > ${MAIN_C_FILE}
	DEPENDS ${COMBINED_DATA_FILE}
)
add_custom_target(${KMODULE_NAME}_module.c
	DEPENDS ${MAIN_C_FILE}
)
add_dependencies(${KMODULE_NAME}_module.c ${KMODULE_NAME}_data_file)

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr_mem_core")

kbuild_add_module(${KMODULE_NAME}
# sources
	${MAIN_C_FILE}
	${HANDLER_C_FILES}
	
# headers
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/functions.h"
	"${TOP_INCLUDE_DIR}/kedr_mem/local_storage.h"
	"${TOP_INCLUDE_DIR}/object_types.h"
	"${CMAKE_BINARY_DIR}/config.h"
)
add_dependencies(${KMODULE_NAME} ${KMODULE_NAME}_module.c)
kedr_install_kmodule(${KMODULE_NAME})
kedr_install_symvers(${KMODULE_NAME})
########################################################################

add_subdirectory(groups)
########################################################################