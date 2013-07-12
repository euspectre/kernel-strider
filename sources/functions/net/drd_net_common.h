/* Common declarations for all the function groups in the plugin. */

#ifndef DRD_NET_COMMON_H_1638_INCLUDED
#define DRD_NET_COMMON_H_1638_INCLUDED

#define KEDR_MSG_PREFIX "[kedr_fh_drd_net] "
/* ====================================================================== */

struct net_device_ops;
struct ethtool_ops;
/* ====================================================================== */

/* Call these functions at the beginning and at the end of a callback,
 * respectively, if the latter is called under rtnl_lock.
 *
 * TODO: revisit, may be pure happens-before is not applicable.
void
kedr_rtnl_lock(unsigned long tid, unsigned long pc);

void
kedr_rtnl_unlock(unsigned long tid, unsigned long pc); */
/* ====================================================================== */

/* Set handlers for net_device_ops callbacks. */
void
kedr_set_net_device_ops_handlers(const struct net_device_ops *ops);

/* Set handlers for ethtool_ops callbacks. */
void
kedr_set_ethtool_ops_handlers(const struct ethtool_ops *ops);
/* ====================================================================== */

#endif /* DRD_NET_COMMON_H_1638_INCLUDED */
