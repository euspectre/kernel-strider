# Handling of the callbacks from struct device_driver (most of the time - 
# power management hooks).
# Unlike many other groups, this group allows to set more than one handler 
# for a given callback too.

check_struct_has_member(
	"struct device_driver" 
	"pm"
	"linux/device.h"
	device_driver_has_pm)

if (NOT device_driver_has_pm)
	message(FATAL_ERROR 
"struct device_driver seems not to have 'pm' field - please check linux/device.h"
	)
endif ()

configure_struct_members("struct dev_pm_ops" "linux/pm.h" callbacks
	"prepare"
	"complete"
	"suspend"
	"resume"
	"freeze"
	"thaw"
	"poweroff"
	"restore"
	"suspend_late"
	"resume_early"
	"freeze_late"
	"thaw_early"
	"poweroff_late"
	"restore_early"
	"suspend_noirq"
	"resume_noirq"
	"freeze_noirq"
	"thaw_noirq"
	"poweroff_noirq"
	"restore_noirq"
	"runtime_suspend"
	"runtime_resume"
	"runtime_idle"
)

set (source_file "device.c")
set (PM_OPS_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/pm_ops.list")

# Save the list of callbacks to a file to be used with kedr_gen
# later.
set (KEDR_PM_OPS_LIST "")
foreach (c ${callbacks})
	set (KEDR_PM_OPS_LIST "${KEDR_PM_OPS_LIST}\tfunction = ${c}\n")
endforeach ()
configure_file("pm_ops.list.in" "${PM_OPS_LIST_FILE}")

# Generate the source file
add_custom_command(OUTPUT ${source_file}
	COMMAND ${KEDR_GEN_TOOL} 
		"${CMAKE_CURRENT_SOURCE_DIR}/templates"
		"${PM_OPS_LIST_FILE}" > ${source_file}
	DEPENDS ${PM_OPS_LIST_FILE}
)

# Add the source file to the list of sources of the plugin.
group_add_sources("${source_file}")	
