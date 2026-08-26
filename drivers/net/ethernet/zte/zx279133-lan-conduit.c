// SPDX-License-Identifier: GPL-2.0-only
/*
 * ZTE ZX279133 XMAC0 LAN conduit netdev.
 *
 * The NPPT driver owns the shared datapath; this child owns the conduit
 * netdev and forwards its operations through the typed parent service.
 */

#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "zx279133-lan.h"

#define zx279133_lan_conduit_priv zx279133_lan_netdev_priv

static int zx279133_lan_conduit_open(struct net_device *ndev)
{
	struct zx279133_lan_conduit_priv *priv = netdev_priv(ndev);

	return zx279133_lan_service_netdev_open(priv->service, ndev);
}

static int zx279133_lan_conduit_stop(struct net_device *ndev)
{
	struct zx279133_lan_conduit_priv *priv = netdev_priv(ndev);

	return zx279133_lan_service_netdev_stop(priv->service, ndev);
}

static netdev_tx_t zx279133_lan_conduit_xmit(struct sk_buff *skb,
					     struct net_device *ndev)
{
	struct zx279133_lan_conduit_priv *priv = netdev_priv(ndev);

	return zx279133_lan_service_netdev_xmit(priv->service, skb, ndev);
}

static void zx279133_lan_conduit_tx_timeout(struct net_device *ndev,
					    unsigned int txqueue)
{
	struct zx279133_lan_conduit_priv *priv = netdev_priv(ndev);

	zx279133_lan_service_netdev_tx_timeout(priv->service, ndev, txqueue);
}

static void
zx279133_lan_conduit_get_stats64(struct net_device *ndev,
				 struct rtnl_link_stats64 *stats)
{
	struct zx279133_lan_conduit_priv *priv = netdev_priv(ndev);

	zx279133_lan_service_netdev_get_stats64(priv->service, ndev, stats);
}

static int zx279133_lan_conduit_change_mtu(struct net_device *ndev,
					   int new_mtu)
{
	struct zx279133_lan_conduit_priv *priv = netdev_priv(ndev);

	return zx279133_lan_service_netdev_change_mtu(priv->service, ndev,
						      new_mtu);
}

static int zx279133_lan_conduit_setup_tc(struct net_device *ndev,
					 enum tc_setup_type type,
					 void *type_data)
{
	struct zx279133_lan_conduit_priv *priv = netdev_priv(ndev);

	return zx279133_lan_service_netdev_setup_tc(priv->service, ndev, type,
						   type_data);
}

static const struct net_device_ops zx279133_lan_conduit_netdev_ops = {
	.ndo_open = zx279133_lan_conduit_open,
	.ndo_stop = zx279133_lan_conduit_stop,
	.ndo_start_xmit = zx279133_lan_conduit_xmit,
	.ndo_tx_timeout = zx279133_lan_conduit_tx_timeout,
	.ndo_get_stats64 = zx279133_lan_conduit_get_stats64,
	.ndo_change_mtu = zx279133_lan_conduit_change_mtu,
	.ndo_setup_tc = zx279133_lan_conduit_setup_tc,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
};

static void zx279133_lan_conduit_teardown(void *data)
{
	struct net_device *ndev = data;
	struct zx279133_lan_conduit_priv *priv = netdev_priv(ndev);

	zx279133_lan_service_netdev_teardown(priv->service, ndev);
}

static int zx279133_lan_conduit_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_lan_conduit_priv *priv;
	struct zx279133_lan_service *service;
	struct net_device *ndev;
	int ret;

	service = dev_get_drvdata(dev->parent);
	if (!zx279133_lan_service_valid(service))
		return dev_err_probe(dev, -EPROBE_DEFER,
				     "NPPT LAN service is unavailable\n");

	ndev = devm_alloc_etherdev(dev, sizeof(*priv));
	if (!ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, dev);
	strscpy(ndev->name, "lan-cpu%d", IFNAMSIZ);
	ndev->netdev_ops = &zx279133_lan_conduit_netdev_ops;
	ndev->tstats = devm_netdev_alloc_pcpu_stats(dev,
						    struct pcpu_sw_netstats);
	if (!ndev->tstats)
		return -ENOMEM;

	priv = netdev_priv(ndev);
	priv->service = service;
	zx279133_netdev_stats_init(&priv->stats);

	ret = zx279133_lan_service_netdev_setup(service, ndev);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to claim LAN conduit netdev\n");

	ret = devm_add_action_or_reset(dev, zx279133_lan_conduit_teardown,
				       ndev);
	if (ret)
		return ret;

	ret = devm_register_netdev(dev, ndev);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to register LAN conduit netdev\n");

	dev_info(dev, "LAN conduit netdev registered\n");
	return 0;
}

static const struct of_device_id zx279133_lan_conduit_of_match[] = {
	{ .compatible = "zte,zx279133-lan-conduit" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_lan_conduit_of_match);

static struct platform_driver zx279133_lan_conduit_driver = {
	.probe = zx279133_lan_conduit_probe,
	.driver = {
		.name = "zx279133-lan-conduit",
		.of_match_table = zx279133_lan_conduit_of_match,
	},
};
module_platform_driver(zx279133_lan_conduit_driver);

MODULE_DESCRIPTION("ZTE ZX279133 XMAC0 LAN conduit");
MODULE_LICENSE("GPL");
