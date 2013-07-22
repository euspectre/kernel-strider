/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Author: 
 *      Eugene A. Shatokhin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/neighbour.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* All the callbacks from struct net_device_ops are affected by the standard
 * happens-before relations w.r.t. register/unregister_netdev*.
 *
 * Besides that, the following synchronization rules are expressed here.
 * 
 * 1. The following callbacks execute under rtnl_lock():
 * - ndo_init
 * - ndo_uninit
 * - ndo_open
 * - ndo_stop
 * - ndo_change_rx_flags
 * - ndo_set_mac_address
 * - ndo_validate_addr
 * - ndo_do_ioctl
 * - ndo_set_config
 * - ndo_change_mtu
 * - ndo_neigh_setup
 * - ndo_vlan_rx_add_vid
 * - ndo_vlan_rx_kill_vid
 * - ndo_netpoll_setup
 * - ndo_netpoll_cleanup
 * - ndo_set_vf_mac
 * - ndo_set_vf_vlan
 * - ndo_set_vf_tx_rate
 * - ndo_set_vf_spoofchk
 * - ndo_get_vf_config
 * - ndo_set_vf_port
 * - ndo_get_vf_port
 * - ndo_setup_tc
 * - ndo_fcoe_enable
 * - ndo_fcoe_disable
 * - ndo_add_slave
 * - ndo_del_slave
 * - ndo_fix_features
 * - ndo_set_features
 * - ndo_fdb_add
 * - ndo_fdb_del
 * - ndo_fdb_dump
 * - ndo_bridge_setlink
 * - ndo_bridge_getlink
 * - ndo_bridge_dellink
 * - ndo_change_carrier
 * - ndo_vlan_rx_register
 *
 * 2. ndo_start_xmit:
 *    Context: BH disabled (IRQ may be disabled as well but not in each case).
 *    Locking: __netif_tx_lock() spinlock for the queue skb has been put to.
 *
 * 3. ndo_select_queue:
 *    Context: BH disabled
 *    Locking: probably none
 *
 * 4. ndo_set_rx_mode, ndo_set_multicast_list:
 *    Context: BH disabled
 *    Locking: netif_addr_lock() spinlock
 *
 * 5. ndo_tx_timeout:
 *    Context: BH disabled
 *    Locking: netif_tx_lock(), i.e. &dev->tx_global_lock spinlock
 *
 * 6. ndo_get_stats64, ndo_get_stats:
 *    Locking: no locks significant to our system (dev_base_lock is taken 
 * for reading but no driver code executes with that lock taken for writing,
 * so it does not impose any sync rules we should handle here).
 *
 * 7. ndo_poll_controller:
 *    Context: BH disabled (at least)
 *    Locking & other sync:
 *    - ndo_poll_controller cannot execute at the same time as .ndo_open, 
 * .ndo_stop, .ndo_validate_addr for the same net_device instance 
 * (netpoll_rx_disable/enable called by the kernel itself in appropriate 
 * places);
 *    - ndo_poll_controller cannot execute concurrently with itself for the
 * same net_device instance.
 * 
 * Can be modelled as follows:
 *      func (ndo_open, etc.)           ndo_poll_controller
 *          HA(id_poll2)                    HA(id_poll3)
 *                                          HA(id_poll4)
 *
 *              ...                             ...
 *
 *                                          HB(id_poll4)
 *          HB(id_poll3)                    HB(id_poll2)
 *
 * id_poll2 = (ulong)netdev + 2
 * id_poll3 = (ulong)netdev + 3
 * id_poll4 = (ulong)netdev + 4
 *
 * 8. ndo_neigh_construct, ndo_neigh_destroy:
 *    It seems, no additional rules here.
 * 
 * 9. Locking is currently not clear for the following callbacks:
 * - ndo_fcoe_ddp_setup
 * - ndo_fcoe_ddp_done
 * - ndo_fcoe_ddp_target
 * - ndo_fcoe_get_hbainfo
 * - ndo_fcoe_get_wwn
 * - ndo_rx_flow_steer
 *
 * [NB] 'struct net_device *' is the first argument of the most callbacks,
 * except:
 * - ndo_start_xmit - it is the 2nd argument;
 * - ndo_neigh_construct, ndo_neigh_destroy - it is in 'struct neighbour *'
 *    which is passed as the first argument;
 * - ndo_fdb_add, ndo_fdb_del, ndo_fdb_dump - it is the 2nd or the 3rd 
 *    argument depending on the kernel version;
 * - ndo_bridge_getlink - it is the 4th argument. */
/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

static struct kedr_fh_group fh_group = {
	.handlers = NULL,
};
/* ====================================================================== */

/* Set the handlers for the net_device_ops callbacks. */
void
kedr_set_net_device_ops_handlers(const struct net_device_ops *ops)
{
<$set_handlers : join(\n)$>}
/* ====================================================================== */

struct kedr_fh_group * __init
kedr_fh_get_group_net_device_ops(void)
{
	/* No handlers of the exported functions, there are only handlers
	 * for the callbacks here. */
	fh_group.num_handlers = 0;
	return &fh_group;
}
/* ====================================================================== */
