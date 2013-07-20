# This group handles the callbacks from struct header_ops.

configure_struct_members("struct header_ops" "linux/netdevice.h" callbacks
	"create"
	"parse"
	"rebuild"
	"cache"
	"cache_update"
)

if (NOT callbacks)
	message (FATAL_ERROR
		"No known callbacks found in struct header_ops.")
endif ()

# We can use the standard API to generate the source file for a group of
# functions here despite that callbacks rather than exported functions are
# to be handled.
group_create_handlers(${GROUP_NAME} ${callbacks})
group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################
