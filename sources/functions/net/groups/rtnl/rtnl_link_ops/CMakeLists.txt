# This subgroup provides handlers for the callbacks from
# struct rtnl_link_ops.
set (source_file "cb_rtnl_link_ops.c")

configure_struct_members("struct rtnl_link_ops" "net/rtnetlink.h" callbacks
	"setup"
	"validate"
	"newlink"
	"changelink"
	"dellink"
	"get_size"
	"fill_info"
	"get_xstats_size"
	"fill_xstats"
	"get_num_tx_queues"
	"get_num_rx_queues"
	"get_tx_queues"
)

if (NOT callbacks)
	message (FATAL_ERROR 
		"No known callbacks found in struct rtnl_link_ops.")
endif ()

set (data_files)
foreach (c ${callbacks})
	list (APPEND data_files "${c}.data")
endforeach ()
to_abs_path(data_files_abs ${data_files})

set (func_data "func.data")

add_custom_command(OUTPUT ${func_data}
	COMMAND cat ${data_files_abs} > ${func_data}
	DEPENDS ${data_files_abs}
)

add_custom_command(OUTPUT ${source_file}
	COMMAND ${KEDR_GEN_TOOL} 
		"${CMAKE_CURRENT_SOURCE_DIR}/templates"
		${func_data} > ${source_file}
	DEPENDS ${func_data}
)

group_add_sources(${source_file})
