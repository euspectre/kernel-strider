# This group handles the callbacks from struct net_device_ops.

configure_struct_members(
"struct net_device_ops" "linux/netdevice.h" callbacks
	"ndo_init"
	"ndo_uninit"
	"ndo_open"
	"ndo_stop"
	"ndo_start_xmit"
	"ndo_select_queue"
	"ndo_change_rx_flags"
	"ndo_set_rx_mode"
	"ndo_set_multicast_list"
	"ndo_set_mac_address"
	"ndo_validate_addr"
	"ndo_do_ioctl"
	"ndo_set_config"
	"ndo_change_mtu"
	"ndo_neigh_setup"
	"ndo_tx_timeout"
	"ndo_get_stats64"
	"ndo_get_stats"
	"ndo_vlan_rx_register"
	"ndo_vlan_rx_add_vid"
	"ndo_vlan_rx_kill_vid"
	"ndo_poll_controller"
	"ndo_netpoll_setup"
	"ndo_netpoll_cleanup"
	"ndo_set_vf_mac"
	"ndo_set_vf_vlan"
	"ndo_set_vf_tx_rate"
	"ndo_set_vf_spoofchk"
	"ndo_get_vf_config"
	"ndo_set_vf_port"
	"ndo_get_vf_port"
	"ndo_setup_tc"
	"ndo_fcoe_enable"
	"ndo_fcoe_disable"
	"ndo_fcoe_ddp_setup"
	"ndo_fcoe_ddp_done"
	"ndo_fcoe_ddp_target"
	"ndo_fcoe_get_hbainfo"
	"ndo_fcoe_get_wwn"
	"ndo_rx_flow_steer"
	"ndo_add_slave"
	"ndo_del_slave"
	"ndo_fix_features"
	"ndo_set_features"
	"ndo_neigh_construct"
	"ndo_neigh_destroy"
	"ndo_fdb_add"
	"ndo_fdb_del"
	"ndo_fdb_dump"
	"ndo_bridge_setlink"
	"ndo_bridge_getlink"
	"ndo_bridge_dellink"
	"ndo_change_carrier"
)

if (NOT callbacks)
	message (FATAL_ERROR
		"No known callbacks found in struct net_device_ops.")
endif ()

# We can use the standard API to generate the source file for a group of
# functions here despite that callbacks rather than exported functions are
# to be handled.
group_create_handlers(${GROUP_NAME} ${callbacks})
group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################
