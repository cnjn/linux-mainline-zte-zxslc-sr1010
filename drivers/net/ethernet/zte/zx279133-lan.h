/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ZX279133_LAN_H
#define __ZX279133_LAN_H

#include <linux/netdevice.h>
#include <linux/types.h>

#include "zx279133-stats.h"

#define ZX279133_LAN_USER_MAX_MTU	1970

/*
 * Service boundary between the NPPT parent and the LAN switch child.
 *
 * The parent retains ownership of the shared MMIO mapping, XMAC lock, and
 * shared datapath. The child accesses them only through this typed service;
 * parent implementation details remain private to the NPPT driver.
 */
struct zx279133_lan_service;

struct zx279133_lan_netdev_priv {
	struct zx279133_lan_service *service;
	struct zx279133_netdev_stats stats;
};

struct zx279133_lan_service_ops {
	u32 (*nppt_read)(struct zx279133_lan_service *service, u32 offset);
	void (*nppt_write)(struct zx279133_lan_service *service, u32 offset,
			   u32 value);
	void (*xmac_lock)(struct zx279133_lan_service *service);
	void (*xmac_unlock)(struct zx279133_lan_service *service);
	int (*datapath_get)(struct zx279133_lan_service *service);
	void (*datapath_set_ready)(struct zx279133_lan_service *service, bool ready);
	int (*datapath_quiesce)(struct zx279133_lan_service *service);
	void (*datapath_put)(struct zx279133_lan_service *service);
	void (*set_dsa_active)(struct zx279133_lan_service *service, bool active);
	int (*netdev_setup)(struct zx279133_lan_service *service,
			    struct net_device *ndev);
	void (*netdev_teardown)(struct zx279133_lan_service *service,
				struct net_device *ndev);
	int (*netdev_open)(struct zx279133_lan_service *service,
			   struct net_device *ndev);
	int (*netdev_stop)(struct zx279133_lan_service *service,
			   struct net_device *ndev);
	netdev_tx_t (*netdev_xmit)(struct zx279133_lan_service *service,
				   struct sk_buff *skb,
				   struct net_device *ndev);
	void (*netdev_tx_timeout)(struct zx279133_lan_service *service,
				  struct net_device *ndev, unsigned int txqueue);
	void (*netdev_get_stats64)(struct zx279133_lan_service *service,
				   struct net_device *ndev,
				   struct rtnl_link_stats64 *stats);
	int (*netdev_change_mtu)(struct zx279133_lan_service *service,
				 struct net_device *ndev, int new_mtu);
	int (*netdev_setup_tc)(struct zx279133_lan_service *service,
			       struct net_device *ndev,
			       enum tc_setup_type type, void *type_data);
};

struct zx279133_lan_service {
	const struct zx279133_lan_service_ops *ops;
};

static inline bool
zx279133_lan_service_valid(const struct zx279133_lan_service *service)
{
	return service && service->ops && service->ops->nppt_read &&
	       service->ops->nppt_write && service->ops->xmac_lock &&
	       service->ops->xmac_unlock && service->ops->datapath_get &&
	       service->ops->datapath_set_ready &&
	       service->ops->datapath_quiesce && service->ops->datapath_put &&
	       service->ops->set_dsa_active &&
	       service->ops->netdev_setup && service->ops->netdev_teardown &&
	       service->ops->netdev_open && service->ops->netdev_stop &&
	       service->ops->netdev_xmit && service->ops->netdev_tx_timeout &&
	       service->ops->netdev_get_stats64 &&
	       service->ops->netdev_change_mtu && service->ops->netdev_setup_tc;
}

static inline u32
zx279133_lan_service_nppt_read(struct zx279133_lan_service *service, u32 offset)
{
	return service->ops->nppt_read(service, offset);
}

static inline void
zx279133_lan_service_nppt_write(struct zx279133_lan_service *service, u32 offset,
				u32 value)
{
	service->ops->nppt_write(service, offset, value);
}

static inline void
zx279133_lan_service_xmac_lock(struct zx279133_lan_service *service)
{
	service->ops->xmac_lock(service);
}

static inline void
zx279133_lan_service_xmac_unlock(struct zx279133_lan_service *service)
{
	service->ops->xmac_unlock(service);
}

static inline int
zx279133_lan_service_datapath_get(struct zx279133_lan_service *service)
{
	return service->ops->datapath_get(service);
}

static inline void
zx279133_lan_service_datapath_set_ready(struct zx279133_lan_service *service,
					bool ready)
{
	service->ops->datapath_set_ready(service, ready);
}

static inline int
zx279133_lan_service_datapath_quiesce(struct zx279133_lan_service *service)
{
	return service->ops->datapath_quiesce(service);
}

static inline void
zx279133_lan_service_datapath_put(struct zx279133_lan_service *service)
{
	service->ops->datapath_put(service);
}

static inline void
zx279133_lan_service_set_dsa_active(struct zx279133_lan_service *service,
				    bool active)
{
	service->ops->set_dsa_active(service, active);
}

static inline int
zx279133_lan_service_netdev_setup(struct zx279133_lan_service *service,
				  struct net_device *ndev)
{
	return service->ops->netdev_setup(service, ndev);
}

static inline void
zx279133_lan_service_netdev_teardown(struct zx279133_lan_service *service,
				     struct net_device *ndev)
{
	service->ops->netdev_teardown(service, ndev);
}

static inline int
zx279133_lan_service_netdev_open(struct zx279133_lan_service *service,
				 struct net_device *ndev)
{
	return service->ops->netdev_open(service, ndev);
}

static inline int
zx279133_lan_service_netdev_stop(struct zx279133_lan_service *service,
				 struct net_device *ndev)
{
	return service->ops->netdev_stop(service, ndev);
}

static inline netdev_tx_t
zx279133_lan_service_netdev_xmit(struct zx279133_lan_service *service,
				 struct sk_buff *skb,
				 struct net_device *ndev)
{
	return service->ops->netdev_xmit(service, skb, ndev);
}

static inline void
zx279133_lan_service_netdev_tx_timeout(struct zx279133_lan_service *service,
				       struct net_device *ndev,
				       unsigned int txqueue)
{
	service->ops->netdev_tx_timeout(service, ndev, txqueue);
}

static inline void
zx279133_lan_service_netdev_get_stats64(struct zx279133_lan_service *service,
					struct net_device *ndev,
					struct rtnl_link_stats64 *stats)
{
	service->ops->netdev_get_stats64(service, ndev, stats);
}

static inline int
zx279133_lan_service_netdev_change_mtu(struct zx279133_lan_service *service,
				       struct net_device *ndev,
				       int new_mtu)
{
	return service->ops->netdev_change_mtu(service, ndev, new_mtu);
}

static inline int
zx279133_lan_service_netdev_setup_tc(struct zx279133_lan_service *service,
				     struct net_device *ndev,
				     enum tc_setup_type type,
				     void *type_data)
{
	return service->ops->netdev_setup_tc(service, ndev, type, type_data);
}

static inline void
zx279133_fill_netdev_stats64(struct net_device *ndev,
			     const struct zx279133_netdev_stats *sw_stats,
				     struct rtnl_link_stats64 *stats)
{
	dev_get_tstats64(ndev, stats);
	stats->rx_errors += atomic64_read(&sw_stats->rx_errors);
	stats->tx_errors += atomic64_read(&sw_stats->tx_errors);
	stats->rx_dropped += atomic64_read(&sw_stats->rx_dropped);
	stats->tx_dropped += atomic64_read(&sw_stats->tx_dropped);
	stats->rx_length_errors += atomic64_read(&sw_stats->rx_length_errors);
}

#endif /* __ZX279133_LAN_H */
