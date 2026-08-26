// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <linux/hash.h>
#include <linux/if_vlan.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ip.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/mfd/syscon.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pcs/pcs-xpcs.h>
#include <linux/phy/phy.h>
#include <linux/phylink.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/tcp.h>
#include <linux/rtnetlink.h>
#include <linux/sizes.h>
#include <linux/unaligned.h>
#include <linux/workqueue.h>

#include <net/page_pool/helpers.h>

#include "zx279133.h"

static const char * const zx279133_clock_names[ZX279133_NUM_CLOCKS] = {
	"idm-aclk",
	"tm-aclk",
	"pclk",
	"smac-wclk",
	"woe1-wclk",
	"mac-wclk",
};

static const char * const zx279133_irq_names[ZX279133_NUM_IRQS] = {
	"nppt",
	"rx",
	"idm",
	"buffer-release",
	"local-test",
};

static void zx279133_xpcs_destroy(void *data)
{
	xpcs_destroy(data);
}

static void zx279133_napi_del(void *data)
{
	netif_napi_del(data);
}

static void zx279133_rx_page_pool_destroy(void *data)
{
	struct zx279133_eth *eth = data;

	WARN_ON(eth->rx_page_map_count);
	page_pool_destroy(eth->rx_page_pool);
}

static void zx279133_mdio_device_put(void *data)
{
	mdio_device_put(data);
}

static void zx279133_phylink_destroy(void *data)
{
	phylink_destroy(data);
}

static int zx279133_idm_cci_enable(struct zx279133_eth *eth)
{
	int ret;

	ret = regmap_read(eth->idm_cci, 0, &eth->idm_cci_saved[0]);
	if (ret)
		return ret;

	ret = regmap_read(eth->idm_cci, 4, &eth->idm_cci_saved[1]);
	if (ret)
		return ret;

	ret = regmap_write(eth->idm_cci, 0, ZX279133_IDM_CCI_VALUE);
	if (ret)
		return ret;

	ret = regmap_write(eth->idm_cci, 4, ZX279133_IDM_CCI_VALUE);
	if (ret)
		regmap_write(eth->idm_cci, 0, eth->idm_cci_saved[0]);

	return ret;
}

static void zx279133_idm_cci_restore(struct zx279133_eth *eth)
{
	regmap_write(eth->idm_cci, 0, eth->idm_cci_saved[0]);
	regmap_write(eth->idm_cci, 4, eth->idm_cci_saved[1]);
}

int zx279133_hardware_prepare(struct zx279133_eth *eth)
{
	int ret;

	ret = zx279133_idm_cci_enable(eth);
	if (ret)
		return ret;

	ret = clk_bulk_prepare_enable(ARRAY_SIZE(eth->clocks), eth->clocks);
	if (ret)
		goto err_cci_restore;

	ret = zx279133_tm_prepare(eth);
	if (ret)
		goto err_clk_disable;
	ret = zx279133_np_prepare(eth);
	if (ret)
		goto err_tm_restore;
	zx279133_route_set(eth, true);
	ret = zx279133_vlan_runtime_prepare(eth);
	if (ret)
		goto err_route_off;
	zx279133_xmac_set_enabled(eth, false);

	/* A failed stop may leave the generic PHY power reference held. Retry
	 * the power-off before programming a new mode so the provider callback
	 * cannot be skipped by the following power-on.
	 */
	if (eth->serdes_powered) {
		ret = phy_power_off(eth->serdes);
		if (ret)
			goto err_route_off;
		eth->serdes_powered = false;
	}

	ret = phy_set_mode_ext(eth->serdes, PHY_MODE_ETHERNET,
			       eth->host_interface);
	if (ret)
		goto err_route_off;

	ret = phy_power_on(eth->serdes);
	if (ret)
		goto err_route_off;
	eth->serdes_powered = true;
	eth->serdes_interface = eth->host_interface;
	usleep_range(2000, 2500);
	eth->hardware_prepared = true;

	return 0;

err_route_off:
	zx279133_route_set(eth, false);
	zx279133_np_restore(eth);
err_tm_restore:
	zx279133_tm_restore(eth);
err_clk_disable:
	clk_bulk_disable_unprepare(ARRAY_SIZE(eth->clocks), eth->clocks);
err_cci_restore:
	zx279133_idm_cci_restore(eth);
	return ret;
}

void zx279133_hardware_unprepare(struct zx279133_eth *eth)
{
	int ret;

	if (!eth->hardware_prepared)
		return;

	zx279133_xmac_set_enabled(eth, false);
	zx279133_flow_offload_flush(eth);
	if (eth->xpcs_runtime_held)
		zx279133_xpcs_set_bypass(eth, false);
	ret = phy_power_off(eth->serdes);
	if (ret) {
		dev_err(eth->dev, "failed to power off SerDes: %d\n", ret);
	} else {
		eth->serdes_powered = false;
		eth->serdes_interface = PHY_INTERFACE_MODE_NA;
	}
	zx279133_route_set(eth, false);
	zx279133_np_restore(eth);
	zx279133_tm_restore(eth);
	clk_bulk_disable_unprepare(ARRAY_SIZE(eth->clocks), eth->clocks);
	zx279133_idm_cci_restore(eth);
	eth->hardware_prepared = false;
}

static int zx279133_eth_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct net_device *ndev;
	struct zx279133_eth *eth;
	struct device_node *pcs_np;
	struct device_node *bmu_np;
	struct device_node *se_hash_np;
	struct device_node *idm_np;
	struct reserved_mem *bmu_rmem;
	struct reserved_mem *se_hash_rmem;
	struct reserved_mem *idm_rmem;
	unsigned int i;
	int ret;

	ndev = devm_alloc_etherdev(dev, sizeof(*eth));
	if (!ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, dev);
	/* Normal completion is interrupt/NAPI driven; one second keeps a lost
	 * completion interrupt below common protocol retry timeouts.
	 */
	ndev->watchdog_timeo = HZ;
	eth = netdev_priv(ndev);
	eth->dev = dev;
	eth->ndev = ndev;
	ndev->tstats = devm_netdev_alloc_pcpu_stats(dev,
						    struct pcpu_sw_netstats);
	if (!ndev->tstats)
		return -ENOMEM;
	zx279133_netdev_stats_init(&eth->stats);
	atomic64_set(&eth->rx_irq_count, 0);
	atomic64_set(&eth->idm_local_irq_count, 0);
	atomic64_set(&eth->rx_refill_retry_work_runs, 0);
	u64_stats_init(&eth->rx_stats_sync);
	spin_lock_init(&eth->tx_lock);
	spin_lock_init(&eth->irq_lock);
	INIT_DELAYED_WORK(&eth->tx_reclaim_work,
			  zx279133_idm_tx_reclaim_work);
	INIT_DELAYED_WORK(&eth->rx_refill_work,
			  zx279133_idm_rx_refill_work);
	mutex_init(&eth->xmac_lock);
	mutex_init(&eth->datapath_lock);
	eth->tx_slots = devm_kcalloc(dev, ZX279133_IDM_TX_DEPTH,
				     sizeof(*eth->tx_slots), GFP_KERNEL);
	if (!eth->tx_slots)
		return -ENOMEM;

	eth->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(eth->base))
		return PTR_ERR(eth->base);
	eth->pps_base = devm_platform_ioremap_resource_byname(pdev, "pps");
	if (IS_ERR(eth->pps_base))
		return PTR_ERR(eth->pps_base);

	bmu_np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!bmu_np)
		return dev_err_probe(dev, -EINVAL,
				     "missing BMU reserved memory\n");
	bmu_rmem = of_reserved_mem_lookup(bmu_np);
	of_node_put(bmu_np);
	if (!bmu_rmem)
		return dev_err_probe(dev, -EINVAL,
				     "invalid BMU reserved memory\n");
	if (bmu_rmem->size < ZX279133_BMU_REQUIRED_SIZE)
		return dev_err_probe(dev, -EINVAL,
				     "BMU reserved memory is too small\n");
	if (upper_32_bits(bmu_rmem->base + ZX279133_BMU_REQUIRED_SIZE - 1))
		return dev_err_probe(dev, -ERANGE,
				     "BMU reserved memory exceeds 32-bit DMA range\n");
	eth->bmu_base = bmu_rmem->base;
	eth->bmu_size = bmu_rmem->size;
	eth->bmu_mem = devm_ioremap_wc(dev, eth->bmu_base, eth->bmu_size);
	if (!eth->bmu_mem)
		return dev_err_probe(dev, -ENOMEM,
				     "failed to map BMU reserved memory\n");

	se_hash_np = of_parse_phandle(dev->of_node, "memory-region", 1);
	if (!se_hash_np)
		return dev_err_probe(dev, -EINVAL,
				     "missing SE hash reserved memory\n");
	se_hash_rmem = of_reserved_mem_lookup(se_hash_np);
	of_node_put(se_hash_np);
	if (!se_hash_rmem)
		return dev_err_probe(dev, -EINVAL,
				     "invalid SE hash reserved memory\n");
	if (se_hash_rmem->size < ZX279133_SE_HASH_REQUIRED_SIZE)
		return dev_err_probe(dev, -EINVAL,
				     "SE hash reserved memory is too small\n");
	if (!IS_ALIGNED(se_hash_rmem->base, SZ_4K) ||
	    upper_32_bits((se_hash_rmem->base +
			   ZX279133_SE_HASH_REQUIRED_SIZE - 1) >> 12))
		return dev_err_probe(dev, -ERANGE,
				     "SE hash reserved memory is not representable\n");
	eth->se_hash_base = se_hash_rmem->base;
	eth->se_hash_size = se_hash_rmem->size;
	eth->se_hash_mem = devm_ioremap_wc(dev, eth->se_hash_base,
					   ZX279133_SE_HASH_REQUIRED_SIZE);
	if (!eth->se_hash_mem)
		return dev_err_probe(dev, -ENOMEM,
				     "failed to map SE hash reserved memory\n");

	idm_np = of_parse_phandle(dev->of_node, "memory-region", 2);
	if (!idm_np)
		return dev_err_probe(dev, -EINVAL,
				     "missing IDM reserved memory\n");
	idm_rmem = of_reserved_mem_lookup(idm_np);
	of_node_put(idm_np);
	if (!idm_rmem)
		return dev_err_probe(dev, -EINVAL,
				     "invalid IDM reserved memory\n");
	eth->idm_base = idm_rmem->base;
	eth->idm_size = idm_rmem->size;
	if (eth->idm_size < ZX279133_IDM_REQUIRED_SIZE)
		return dev_err_probe(dev, -EINVAL,
				     "IDM reserved memory is too small\n");
	if (eth->idm_base > U32_MAX ||
	    eth->idm_size - 1 > U32_MAX - eth->idm_base)
		return dev_err_probe(dev, -ERANGE,
				     "IDM reserved memory exceeds 32-bit DMA range\n");
	eth->idm_mem = devm_ioremap_wc(dev, eth->idm_base, eth->idm_size);
	if (!eth->idm_mem)
		return dev_err_probe(dev, -ENOMEM,
				     "failed to map IDM reserved memory\n");
	eth->pon_route = syscon_regmap_lookup_by_phandle(dev->of_node,
							 "zte,pon-route");
	if (IS_ERR(eth->pon_route))
		return dev_err_probe(dev, PTR_ERR(eth->pon_route),
				     "failed to get PON route syscon\n");

	eth->idm_cci = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "zte,idm-cci");
	if (IS_ERR(eth->idm_cci))
		return dev_err_probe(dev, PTR_ERR(eth->idm_cci),
				     "failed to get IDM CCI syscon\n");

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return dev_err_probe(dev, ret, "32-bit DMA is unavailable\n");
	{
		size_t rx_descs_size = sizeof(*eth->rx_descs) *
			ZX279133_IDM_RX_QUEUES * ZX279133_IDM_RX_RING_SIZE;
		size_t rx_normal_bp_size = ZX279133_IDM_FREE_RING_SIZE;
		size_t tx_descs_size = sizeof(*eth->tx_descs) *
			ZX279133_IDM_TX_QUEUES * ZX279133_IDM_TX_DEPTH;

		eth->rx_descs = dmam_alloc_coherent(dev, rx_descs_size,
						    &eth->rx_descs_dma,
						 GFP_KERNEL);
		if (!eth->rx_descs)
			return -ENOMEM;
		if (upper_32_bits(eth->rx_descs_dma + rx_descs_size - 1))
			return dev_err_probe(dev, -ERANGE,
					     "RX coherent descriptor rings are above 4GiB\n");

		eth->rx_normal_bp = dmam_alloc_coherent(dev, rx_normal_bp_size,
							&eth->rx_normal_bp_dma,
						      GFP_KERNEL);
		if (!eth->rx_normal_bp)
			return -ENOMEM;
		if (upper_32_bits(eth->rx_normal_bp_dma +
				  rx_normal_bp_size - 1))
			return dev_err_probe(dev, -ERANGE,
					     "RX coherent free ring is above 4GiB\n");

		eth->tx_descs = dmam_alloc_coherent(dev, tx_descs_size,
						    &eth->tx_descs_dma,
						 GFP_KERNEL);
		if (!eth->tx_descs)
			return -ENOMEM;
		if (upper_32_bits(eth->tx_descs_dma + tx_descs_size - 1))
			return dev_err_probe(dev, -ERANGE,
					     "TX coherent descriptor ring is above 4GiB\n");
	}
	if (zx279133_tx_in_window) {
		size_t tx_payload_size = ZX279133_IDM_TX_DEPTH *
			ZX279133_IDM_TX_PAYLOAD_STRIDE;

		eth->tx_payload = dmam_alloc_coherent(dev, tx_payload_size,
						      &eth->tx_payload_dma,
						      GFP_KERNEL);
		if (!eth->tx_payload)
			return -ENOMEM;
		if (upper_32_bits(eth->tx_payload_dma + tx_payload_size - 1))
			return dev_err_probe(dev, -ERANGE,
					     "TX coherent payload is above 4GiB\n");
	}
	for (i = 0; i < ARRAY_SIZE(eth->clocks); i++)
		eth->clocks[i].id = zx279133_clock_names[i];

	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(eth->clocks), eth->clocks);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get clocks\n");

	for (i = 0; i < ARRAY_SIZE(eth->irqs); i++) {
		eth->irqs[i] = platform_get_irq_byname(pdev, zx279133_irq_names[i]);
		if (eth->irqs[i] < 0)
			return dev_err_probe(dev, eth->irqs[i],
					     "failed to get %s IRQ\n",
					     zx279133_irq_names[i]);
	}

	eth->serdes = devm_phy_get(dev, "serdes");
	if (IS_ERR(eth->serdes))
		return dev_err_probe(dev, PTR_ERR(eth->serdes),
				     "failed to get SerDes PHY\n");

	pcs_np = of_parse_phandle(dev->of_node, "pcs-handle", 0);
	if (!pcs_np)
		return dev_err_probe(dev, -ENODEV, "PCS phandle is missing\n");

	eth->xpcs_mdiodev = fwnode_mdio_find_device(of_fwnode_handle(pcs_np));
	if (!eth->xpcs_mdiodev) {
		of_node_put(pcs_np);
		return dev_err_probe(dev, -EPROBE_DEFER,
				     "XPCS MDIO device is unavailable\n");
	}

	ret = devm_add_action_or_reset(dev, zx279133_mdio_device_put,
				       eth->xpcs_mdiodev);
	if (ret) {
		of_node_put(pcs_np);
		return ret;
	}

	eth->xpcs = xpcs_create_fwnode(of_fwnode_handle(pcs_np));
	of_node_put(pcs_np);
	if (IS_ERR(eth->xpcs))
		return dev_err_probe(dev, PTR_ERR(eth->xpcs),
				     "failed to create XPCS handle\n");

	ret = devm_add_action_or_reset(dev, zx279133_xpcs_destroy, eth->xpcs);
	if (ret)
		return ret;

	ret = device_get_phy_mode(dev);
	if (ret < 0)
		return dev_err_probe(dev, ret,
				     "failed to get initial PHY interface mode\n");
	eth->host_interface = ret;

	eth->phylink_config.dev = &ndev->dev;
	eth->phylink_config.type = PHYLINK_NETDEV;
	/*
	 * The factory XMAC1 path leaves 802.3x pause disabled. Advertising it
	 * enables RX_FLOW, which throttles WAN transmit under forwarding load.
	 */
	eth->phylink_config.mac_capabilities =
		MAC_2500FD | MAC_1000FD |
		MAC_100 | MAC_10;
	__set_bit(PHY_INTERFACE_MODE_2500BASEX,
		  eth->phylink_config.supported_interfaces);
	__set_bit(PHY_INTERFACE_MODE_SGMII,
		  eth->phylink_config.supported_interfaces);

	eth->phylink = phylink_create(&eth->phylink_config, dev_fwnode(dev),
				      eth->host_interface,
				      &zx279133_phylink_ops);
	if (IS_ERR(eth->phylink))
		return dev_err_probe(dev, PTR_ERR(eth->phylink),
				     "failed to create phylink\n");

	ret = devm_add_action_or_reset(dev, zx279133_phylink_destroy,
				       eth->phylink);
	if (ret)
		return ret;
	ret = zx279133_flow_offload_init(eth);
	if (ret)
		return ret;

	ndev->netdev_ops = &zx279133_netdev_ops;
	ndev->ethtool_ops = &zx279133_ethtool_ops;
	ndev->priv_flags |= IFF_LIVE_ADDR_CHANGE;
	if (zx279133_tx_hw_csum) {
		/* Non-IPv4-TCP partial checksums use skb_checksum_help(). */
		ndev->hw_features |= NETIF_F_HW_CSUM;
		ndev->features |= NETIF_F_HW_CSUM;
	}
	ndev->hw_features |= NETIF_F_HW_TC;
	ndev->features |= NETIF_F_HW_TC;
	ndev->min_mtu = ETH_MIN_MTU;
	ndev->max_mtu = ZX279133_MAX_MTU;
	if (of_get_ethdev_address(dev->of_node, ndev))
		eth_hw_addr_random(ndev);
	netif_carrier_off(ndev);

	netif_napi_add(ndev, &eth->napi, zx279133_idm_rx_poll);
	ret = devm_add_action_or_reset(dev, zx279133_napi_del, &eth->napi);
	if (ret)
		return ret;

	{
		struct page_pool_params params = {
			.order = ZX279133_RX_PAGE_ORDER,
			.pool_size = ZX279133_IDM_RX_BUFFER_COUNT,
			.nid = NUMA_NO_NODE,
			.dev = dev,
			.napi = &eth->napi,
			.dma_dir = DMA_FROM_DEVICE,
			.max_len = ZX279133_IDM_RX_FRAME_LIMIT,
			.offset = ZX279133_IDM_RX_PAYLOAD_OFFSET,
			.netdev = ndev,
			.queue_idx = 0,
			.flags = PP_FLAG_DMA_MAP | PP_FLAG_DMA_SYNC_DEV,
		};

		eth->rx_page_map = devm_kcalloc(dev,
						ZX279133_IDM_RX_PAGE_MAP_SIZE,
					       sizeof(*eth->rx_page_map),
					       GFP_KERNEL);
		if (!eth->rx_page_map)
			return -ENOMEM;
		eth->rx_page_pool = page_pool_create(&params);
		if (IS_ERR(eth->rx_page_pool)) {
			ret = PTR_ERR(eth->rx_page_pool);
			return dev_err_probe(dev, ret,
					     "failed to create RX page pool\n");
		}
		ret = devm_add_action_or_reset(dev,
					       zx279133_rx_page_pool_destroy, eth);
		if (ret)
			return ret;
	}

	/*
	 * Source 0 (direct CPU RX) and source 2 (vendor "localtest") share
	 * this driver's NAPI instance. The NPPT aggregate source has no
	 * recovered acknowledge contract, source 1 belongs to the unused
	 * companion/Wi-Fi path, and source 3 belongs to the vendor buffer
	 * release callback, so those three DT IRQs remain masked/unrequested.
	 */
	ret = devm_request_irq(dev, eth->irqs[1], zx279133_idm_rx_irq,
			       IRQF_NO_AUTOEN, dev_name(dev), eth);
	if (ret)
		return dev_err_probe(dev, ret, "failed to request RX IRQ\n");

	ret = devm_request_irq(dev, eth->irqs[4], zx279133_idm_local_irq,
			       IRQF_NO_AUTOEN, "zx279133-idm-local", eth);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to request IDM local-test IRQ\n");

	ret = devm_register_netdev(dev, ndev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register netdev\n");

	eth->lan_service.ops = &zx279133_lan_service_ops;
	platform_set_drvdata(pdev, &eth->lan_service);

	ret = devm_of_platform_populate(dev);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to populate LAN child devices\n");

	dev_info(dev, "WAN netdev and LAN child devices registered\n");

	return 0;
}

static const struct of_device_id zx279133_eth_of_match[] = {
	{ .compatible = "zte,zx279133-nppt" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_eth_of_match);

static struct platform_driver zx279133_eth_driver = {
	.probe = zx279133_eth_probe,
	.driver = {
		.name = "zx279133-eth",
		.of_match_table = zx279133_eth_of_match,
	},
};
module_platform_driver(zx279133_eth_driver);

MODULE_FIRMWARE("zte/zx279133/mcode_intel.bin");
MODULE_DESCRIPTION("ZTE ZX279133 NPPT Ethernet driver");
MODULE_LICENSE("GPL");
