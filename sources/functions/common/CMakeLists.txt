# Create the kernel module to handle the function calls relevant for
# data race detection. The functions handlers here can be used by different
# kernel subsystems, hence "common" in the name.

set(PLUGIN_SHORT_NAME "common")

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
	"timer"
	"workqueue"
	"irq"
	"bh"
	"tasklet"
	"device" # [NB] callbacks only
#	"pci"
)
########################################################################

foreach(group ${FUNCTION_GROUPS})
	set(GROUP_NAME ${group})
	add_subdirectory(groups/${group})
endforeach(group ${FUNCTION_GROUPS})
########################################################################

add_fh_drd_plugin(${PLUGIN_SHORT_NAME} ${FUNCTION_GROUPS})
########################################################################
