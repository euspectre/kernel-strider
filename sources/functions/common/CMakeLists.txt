# Create the kernel module to handle the function calls relevant for
# data race detection. The functions handlers here can be used by different
# kernel subsystems, hence "common" in the name.

set(PLUGIN_SHORT_NAME "common")

# The following function groups must always be processed.
set(FUNCTION_GROUPS
	"annotations"
	"alloc" 
	"realloc"
	"free"
	"lock"
	"unlock"
	"uaccess"
	"dup"
	"strings"
	"thread"
	"timer"
	"workqueue"
	"irq"
	"bh"
	"tasklet"
	"device" # [NB] callbacks only
	"pci"
	"firmware"
	"completion"
	"semaphore"
)
########################################################################

foreach(group ${FUNCTION_GROUPS})
	set(GROUP_NAME ${group})
	add_subdirectory(groups/${group})
endforeach(group ${FUNCTION_GROUPS})
########################################################################

set (source_file "module.c")
set (GROUP_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/group.list")

# Save the list of groups to a file to be used with kedr_gen.
set (KEDR_FH_GROUP_LIST "")
foreach (gr ${FUNCTION_GROUPS})
	set (KEDR_FH_GROUP_LIST "${KEDR_FH_GROUP_LIST}\tgroup = ${gr}\n")
endforeach ()
configure_file("group.list.in" "${GROUP_LIST_FILE}")

# Generate the source file
add_custom_command(OUTPUT ${source_file}
	COMMAND ${KEDR_GEN_TOOL} 
		"${CMAKE_CURRENT_SOURCE_DIR}/templates"
		"${GROUP_LIST_FILE}" > ${source_file}
	DEPENDS ${GROUP_LIST_FILE}
)

# Add the source file to the list of sources of the plugin.
group_add_sources("${source_file}")
########################################################################

add_fh_drd_plugin(${PLUGIN_SHORT_NAME} ${FUNCTION_GROUPS})
########################################################################
