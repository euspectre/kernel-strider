kmodule_configure_kernel_functions(functions OPTIONAL
	"request_firmware_nowait"
)

# Check the signature of request_firmware_nowait().
check_request_fw()

group_create_handlers(${GROUP_NAME} ${functions})
group_add_sources("handlers_${GROUP_NAME}.c")
########################################################################
