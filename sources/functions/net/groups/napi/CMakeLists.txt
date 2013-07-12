# This group handles NAPI-related functions.

kmodule_configure_kernel_functions(functions OPTIONAL
	"netif_napi_add"
	"netif_napi_del"
)
group_create_handlers(${GROUP_NAME} ${functions})

group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################
