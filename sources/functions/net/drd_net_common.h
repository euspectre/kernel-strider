/* Common declarations for all the function groups in the plugin. */

#ifndef DRD_NET_COMMON_H_1638_INCLUDED
#define DRD_NET_COMMON_H_1638_INCLUDED

#define KEDR_MSG_PREFIX "[kedr_fh_drd_net] "
/* ====================================================================== */

struct net_device_ops;
struct ethtool_ops;

struct kedr_local_storage;
/* ====================================================================== */

/* Bit masks for different kinds of locks that can be used to track the
 * status of the locks in local_storage::lock_status field. */
#define KEDR_LOCK_MASK_BASE (0x1UL)

/* rtnl_lock */
#define KEDR_LOCK_MASK_RTNL (KEDR_LOCK_MASK_BASE) 

/* netif_addr_lock */
#define KEDR_LOCK_MASK_ADDR (KEDR_LOCK_MASK_BASE << 1)

/* __netif_tx_lock for a given Tx queue */
#define KEDR_LOCK_MASK_TX (KEDR_LOCK_MASK_BASE << 2)

/* netif_tx_lock, i.e. the locks for all Tx queues */
#define KEDR_LOCK_MASK_TX_ALL (KEDR_LOCK_MASK_BASE << 3)
/* ====================================================================== */

/* Call these functions at the beginning and at the end of a callback,
 * respectively, if the latter is called under rtnl_lock. The calls may be
 * nested, they also may be made if the driver has locked rtnl_lock itself.
 * Our system will recognize all these conditions and will emit lock/unlock
 * events for rtnl_lock only if needed. */
void
kedr_rtnl_locked_start(struct kedr_local_storage *ls, unsigned long pc);

void
kedr_rtnl_locked_end(struct kedr_local_storage *ls, unsigned long pc);
/* ====================================================================== */

/* Set handlers for net_device_ops callbacks. */
void
kedr_set_net_device_ops_handlers(const struct net_device_ops *ops);

/* Set handlers for ethtool_ops callbacks. */
void
kedr_set_ethtool_ops_handlers(const struct ethtool_ops *ops);
/* ====================================================================== */

#endif /* DRD_NET_COMMON_H_1638_INCLUDED */
