# This group handles the callbacks from struct ethtool_ops.

configure_struct_members("struct ethtool_ops" "linux/ethtool.h" callbacks
	"get_settings"
	"set_settings"
	"get_drvinfo"
	"get_regs_len"
	"get_regs"
	"get_wol"
	"set_wol"
	"get_msglevel"
	"set_msglevel"
	"nway_reset"
	"get_link"
	"get_eeprom_len"
	"get_eeprom"
	"set_eeprom"
	"get_coalesce"
	"set_coalesce"
	"get_ringparam"
	"set_ringparam"
	"get_pauseparam"
	"set_pauseparam"
	"get_rx_csum"
	"set_rx_csum"
	"get_tx_csum"
	"set_tx_csum"
	"get_sg"
	"set_sg"
	"get_tso"
	"set_tso"
	"self_test"
	"get_strings"
	"phys_id"
	"set_phys_id"
	"get_ethtool_stats"
	"begin"
	"complete"
	"get_ufo"
	"set_ufo"
	"get_flags"
	"set_flags"
	"get_priv_flags"
	"set_priv_flags"
	"get_sset_count"
	"get_rxnfc"
	"set_rxnfc"
	"flash_device"
	"reset"
	"get_rxfh_indir_size"
	"get_rxfh_indir"
	"set_rxfh_indir"
	"get_channels"
	"set_channels"
	"get_dump_flag"
	"get_dump_data"
	"set_dump"
	"get_ts_info"
	"get_module_info"
	"get_module_eeprom"
	"get_eee"
	"set_eee"
)

set (source_file "handlers_${GROUP_NAME}.c")
set (CALLBACKS_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/callbacks.list")

# Save the list of callbacks to a file to be used with kedr_gen
# later.
set (KEDR_CALLBACKS_LIST "")
foreach (c ${callbacks})
	set (KEDR_CALLBACKS_LIST "${KEDR_CALLBACKS_LIST}\tfunction = ${c}\n")
endforeach ()
configure_file("callbacks.list.in" "${CALLBACKS_LIST_FILE}")

# Generate the source file
add_custom_command(OUTPUT ${source_file}
	COMMAND ${KEDR_GEN_TOOL}
		"${CMAKE_CURRENT_SOURCE_DIR}/templates"
		"${CALLBACKS_LIST_FILE}" > ${source_file}
	DEPENDS ${CALLBACKS_LIST_FILE}
)

# Add the source file to the list of sources of the plugin.
group_add_sources("${source_file}")
############################################################################
