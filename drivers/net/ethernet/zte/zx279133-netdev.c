// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/filter.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/io.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/phylink.h>
#include <linux/pm_runtime.h>
#include <linux/rtnetlink.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>

#include <net/page_pool/helpers.h>
#include <net/xdp.h>
#include <net/xdp_sock_drv.h>

#include "zx279133.h"
#include "zx279133-lan.h"

static int zx279133_shared_idm_prepare(struct zx279133_eth *eth)
{
	int ret;

	ret = zx279133_idm_tx_prepare(eth);
	if (ret)
		return ret;
	eth->tx_stopping = false;

	ret = zx279133_idm_rx_prepare(eth);
	if (ret) {
		zx279133_idm_tx_release(eth, true);
		return ret;
	}

	page_pool_enable_direct_recycling(eth->rx_page_pool, &eth->napi);
	page_pool_enable_direct_recycling(eth->lan_rx_page_pool, &eth->lan_napi);
	napi_enable(&eth->lan_napi);
	napi_enable(&eth->napi);
	WRITE_ONCE(eth->napi_enabled, true);

	return 0;
}

static void zx279133_shared_idm_abort(struct zx279133_eth *eth)
{
	napi_disable(&eth->lan_napi);
	napi_disable(&eth->napi);
	WRITE_ONCE(eth->napi_enabled, false);
	page_pool_disable_direct_recycling(eth->rx_page_pool);
	page_pool_disable_direct_recycling(eth->lan_rx_page_pool);
	zx279133_idm_rx_release(eth);
	zx279133_idm_tx_release(eth, true);
}

static void zx279133_shared_rx_start(struct zx279133_eth *eth)
{
	spin_lock_irq(&eth->irq_lock);
	WRITE_ONCE(eth->rx_running, true);
	spin_unlock_irq(&eth->irq_lock);
	enable_irq(eth->irqs[1]);
	eth->rx_irq_enabled = true;
	enable_irq(eth->irqs[4]);
	eth->idm_local_irq_enabled = true;
	spin_lock_irq(&eth->irq_lock);
	zx279133_idm_set_masked(eth, ZX279133_IDM_NAPI_MASK, false);
	spin_unlock_irq(&eth->irq_lock);
}

static void zx279133_shared_rx_stop(struct zx279133_eth *eth)
{
	unsigned long flags;

	spin_lock_irqsave(&eth->irq_lock, flags);
	WRITE_ONCE(eth->rx_running, false);
	zx279133_idm_set_masked(eth, ZX279133_IDM_NAPI_MASK, true);
	spin_unlock_irqrestore(&eth->irq_lock, flags);
	cancel_delayed_work_sync(&eth->rx_refill_work);
	if (eth->idm_local_irq_enabled) {
		disable_irq(eth->irqs[4]);
		eth->idm_local_irq_enabled = false;
	}
	if (eth->rx_irq_enabled) {
		disable_irq(eth->irqs[1]);
		eth->rx_irq_enabled = false;
	}
	if (eth->napi_enabled) {
		napi_disable(&eth->lan_napi);
		napi_disable(&eth->napi);
		WRITE_ONCE(eth->napi_enabled, false);
		page_pool_disable_direct_recycling(eth->rx_page_pool);
		page_pool_disable_direct_recycling(eth->lan_rx_page_pool);
	}
}

static bool zx279133_shared_tx_pause(struct zx279133_eth *eth)
{
	/* Publish the quiesce state before stopping either logical queue so TX
	 * completion reclaim cannot wake one between the two disable calls.
	 */
	WRITE_ONCE(eth->tx_stopping, true);
	if (netif_running(eth->ndev))
		netif_tx_disable(eth->ndev);
	if (eth->lan_ndev && netif_running(eth->lan_ndev))
		netif_tx_disable(eth->lan_ndev);
	cancel_delayed_work_sync(&eth->tx_reclaim_work);

	return zx279133_idm_tx_drain(eth);
}

static void zx279133_shared_tx_resume(struct zx279133_eth *eth)
{
	bool rearm;

	WRITE_ONCE(eth->tx_stopping, false);
	spin_lock_bh(&eth->tx_lock);
	rearm = zx279133_idm_tx_pending(eth);
	spin_unlock_bh(&eth->tx_lock);
	if (rearm)
		mod_delayed_work(system_wq, &eth->tx_reclaim_work,
				 msecs_to_jiffies(ZX279133_TX_RECLAIM_DELAY_MS));
	if ((eth->datapath_users & ZX279133_DATAPATH_USER_WAN) &&
	    netif_running(eth->ndev))
		netif_tx_wake_all_queues(eth->ndev);
	if ((eth->datapath_users & ZX279133_DATAPATH_USER_LAN) &&
	    eth->lan_datapath_ready && eth->lan_ndev &&
	    netif_running(eth->lan_ndev))
		netif_tx_wake_all_queues(eth->lan_ndev);
}

static void zx279133_shared_idm_release(struct zx279133_eth *eth,
					struct net_device *ndev,
					bool tx_quiesced)
{
	if (!tx_quiesced) {
		netdev_warn(ndev,
			    "TX did not quiesce; shutting down hardware before release\n");
		zx279133_xmac_set_enabled(eth, false);
		zx279133_route_set(eth, false);
		/* Stop future IDM fetches from the DMA-coherent descriptor ring
		 * while its clocks are still available. Pending payload mappings are
		 * released only after the remaining NPPT pipeline is shut down.
		 */
		zx279133_idm_tx_deactivate(eth);
		zx279133_hardware_unprepare(eth);
	}
	zx279133_idm_tx_release(eth, tx_quiesced);
	if (tx_quiesced)
		zx279133_hardware_unprepare(eth);
	zx279133_idm_rx_release(eth);
}

static int zx279133_open(struct net_device *ndev)
{
	struct zx279133_eth *eth = netdev_priv(ndev);
	bool first_user;
	int ret;

	mutex_lock(&eth->datapath_lock);
	if (eth->datapath_users & ZX279133_DATAPATH_USER_WAN) {
		ret = -EBUSY;
		goto out_unlock;
	}
	first_user = !eth->datapath_users;

	if (first_user) {
		ret = zx279133_hardware_prepare(eth);
		if (ret)
			goto out_unlock;

		/* The XPCS platform runtime reference belongs to the shared
		 * hardware lifetime. hardware_unprepare() clears PCS bypass and
		 * must run before the last reference is dropped.
		 */
		ret = pm_runtime_resume_and_get(eth->xpcs_mdiodev->bus->parent);
		if (ret < 0)
			goto err_hardware_unprepare;
		eth->xpcs_runtime_held = true;

		ret = zx279133_shared_idm_prepare(eth);
		if (ret)
			goto err_hardware_unprepare;
	}

	ret = phylink_of_phy_connect(eth->phylink, ndev->dev.parent->of_node, 0);
	if (ret)
		goto err_idm_abort;

	phylink_start(eth->phylink);
	if (first_user)
		zx279133_shared_rx_start(eth);
	eth->datapath_users |= ZX279133_DATAPATH_USER_WAN;
	netif_tx_start_all_queues(ndev);
	mutex_unlock(&eth->datapath_lock);

	return 0;

err_idm_abort:
	if (first_user)
		zx279133_shared_idm_abort(eth);
err_hardware_unprepare:
	if (first_user) {
		zx279133_hardware_unprepare(eth);
		if (eth->xpcs_runtime_held) {
			pm_runtime_put(eth->xpcs_mdiodev->bus->parent);
			eth->xpcs_runtime_held = false;
		}
	}
out_unlock:
	mutex_unlock(&eth->datapath_lock);
	return ret;
}

static int zx279133_stop(struct net_device *ndev)
{
	struct zx279133_eth *eth = netdev_priv(ndev);
	bool last_user;
	bool tx_quiesced;

	mutex_lock(&eth->datapath_lock);
	last_user = eth->datapath_users == ZX279133_DATAPATH_USER_WAN;
	if (last_user)
		zx279133_shared_rx_stop(eth);
	tx_quiesced = zx279133_shared_tx_pause(eth);
	phylink_stop(eth->phylink);
	phylink_disconnect_phy(eth->phylink);
	eth->datapath_users &= ~ZX279133_DATAPATH_USER_WAN;
	if (last_user) {
		zx279133_shared_idm_release(eth, ndev, tx_quiesced);
		if (eth->xpcs_runtime_held) {
			pm_runtime_put(eth->xpcs_mdiodev->bus->parent);
			eth->xpcs_runtime_held = false;
		}
	} else {
		if (!tx_quiesced)
			netdev_warn(ndev,
				    "shared TX did not quiesce while LAN remains active\n");
		zx279133_shared_tx_resume(eth);
	}
	mutex_unlock(&eth->datapath_lock);

	return 0;
}

static bool zx279133_tx_csum_supported(struct sk_buff *skb)
{
	const struct iphdr *iph;
	unsigned int network = skb_network_offset(skb);
	unsigned int transport = skb_transport_offset(skb);

	if (skb->protocol != htons(ETH_P_IP) || skb->encapsulation ||
	    network > 63 || transport > 63 ||
	    skb_checksum_start_offset(skb) != transport ||
	    skb->csum_offset != offsetof(struct tcphdr, check) ||
	    skb_csum_is_sctp(skb))
		return false;
	if (!pskb_may_pull(skb, transport + sizeof(struct tcphdr)))
		return false;
	iph = ip_hdr(skb);
	/* DSA may have padded a short frame before it reaches the conduit.
	 * IDM must not include that Ethernet padding in its TCP checksum.
	 */
	return iph->version == 4 && iph->ihl >= 5 &&
		skb->len - network == ntohs(iph->tot_len) &&
		transport == network + iph->ihl * 4 &&
		iph->protocol == IPPROTO_TCP;
}

static netdev_tx_t zx279133_start_xmit_common(struct sk_buff *skb,
					      struct net_device *hw_ndev,
					      struct net_device *ndev)
{
	struct zx279133_eth *eth = netdev_priv(hw_ndev);
	unsigned int queue = skb_get_queue_mapping(skb);
	struct zx279133_tx_ring *tx = &eth->tx[queue];
	struct netdev_queue *txq = netdev_get_tx_queue(ndev, queue);
	struct zx279133_idm_desc *desc;
	struct zx279133_tx_slot *slot;
	dma_addr_t dma;
	u16 producer;
	u8 tx_port;
	bool arm_reclaim;
	bool hw_csum = false;
	bool sw_csum = false;

	spin_lock_bh(&eth->tx_lock);
	if (unlikely(tx->pending >= ZX279133_IDM_TX_DEPTH - 1))
		zx279133_idm_tx_reclaim_queue_locked(eth, queue);
	if (unlikely(tx->pending >= ZX279133_IDM_TX_DEPTH - 1)) {
		netif_stop_subqueue(ndev, queue);
		txq_trans_update(ndev, txq);
		zx279133_idm_tx_flush_queue_locked(eth, queue);
		spin_unlock_bh(&eth->tx_lock);
		return NETDEV_TX_BUSY;
	}
	spin_unlock_bh(&eth->tx_lock);

	if (skb_linearize(skb))
		goto drop;
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		hw_csum = zx279133_tx_hw_csum &&
			skb->len >= ETH_ZLEN &&
			zx279133_tx_csum_supported(skb);
		if (!hw_csum) {
			if (skb_checksum_help(skb))
				goto drop;
			sw_csum = true;
		}
	}
	if (skb_put_padto(skb, ETH_ZLEN)) {
		spin_lock_bh(&eth->tx_lock);
		zx279133_idm_tx_flush_queue_locked(eth, queue);
		spin_unlock_bh(&eth->tx_lock);
		zx279133_stats_tx_dropped(eth, ndev);
		return NETDEV_TX_OK;
	}
	tx_port = ndev != hw_ndev ? ZX279133_LAN_TX_PORT : zx279133_tx_port;

	dma = dma_map_single(eth->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(eth->dev, dma))
		goto drop;

	spin_lock_bh(&eth->tx_lock);
	if (unlikely(tx->pending >= ZX279133_IDM_TX_DEPTH - 1))
		zx279133_idm_tx_reclaim_queue_locked(eth, queue);
	if (unlikely(tx->pending >= ZX279133_IDM_TX_DEPTH - 1)) {
		netif_stop_subqueue(ndev, queue);
		txq_trans_update(ndev, txq);
		zx279133_idm_tx_flush_queue_locked(eth, queue);
		spin_unlock_bh(&eth->tx_lock);
		dma_unmap_single(eth->dev, dma, skb->len, DMA_TO_DEVICE);
		zx279133_stats_tx_dropped(eth, ndev);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	producer = tx->producer;
	desc = eth->tx_descs + (ZX279133_IDM_CPU_TX_FIRST + queue) *
		ZX279133_IDM_TX_DEPTH + producer;
	slot = &tx->slots[producer];
	memset(desc, 0, sizeof(*desc));
	desc->address = cpu_to_le32(lower_32_bits(dma));
	{
		u32 length_flags = (skb->len << 1) |
				   (zx279133_tx_selector & 0xff) << 24 |
				   (zx279133_tx_word4_bit23 ? BIT(23) : 0);
		u32 metadata0 = ZX279133_IDM_TX_CONTROL |
				(tx_port & 0xff) << 16;

		if (hw_csum) {
			length_flags |= BIT(15);
			metadata0 |= (skb_network_offset(skb) * 2) & 0x7f;
			metadata0 |= ((skb_transport_offset(skb) * 2) & 0x7f) << 8;
		}
		desc->length_flags = cpu_to_le32(length_flags);
		desc->metadata[0] = cpu_to_le32(metadata0);
	}
	desc->metadata[4] = cpu_to_le32(zx279133_tx_pon_control);
	slot->skb = skb;
	slot->ndev = ndev;
	slot->dma = dma;
	slot->len = skb->len;
	slot->dma_mapped = true;
	if (hw_csum)
		eth->tx_hw_csum_packets++;
	else if (sw_csum)
		eth->tx_sw_csum_packets++;
	arm_reclaim = !tx->pending;
	tx->producer = (tx->producer + 1) &
				   (ZX279133_IDM_TX_DEPTH - 1);
	tx->pending++;
	tx->notify_pending++;
	if (tx->pending >= ZX279133_IDM_TX_DEPTH - 1) {
		netif_stop_subqueue(ndev, queue);
		txq_trans_update(ndev, txq);
	}
	if (__netdev_tx_sent_queue(txq, skb->len, netdev_xmit_more()))
		zx279133_idm_tx_flush_queue_locked(eth, queue);
	spin_unlock_bh(&eth->tx_lock);

	if (arm_reclaim)
		mod_delayed_work(system_wq, &eth->tx_reclaim_work,
				 msecs_to_jiffies(ZX279133_TX_RECLAIM_DELAY_MS));

	return NETDEV_TX_OK;

drop:
	spin_lock_bh(&eth->tx_lock);
	zx279133_idm_tx_flush_queue_locked(eth, queue);
	spin_unlock_bh(&eth->tx_lock);
	zx279133_stats_tx_dropped(eth, ndev);
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

static netdev_tx_t zx279133_start_xmit(struct sk_buff *skb,
				       struct net_device *ndev)
{
	return zx279133_start_xmit_common(skb, ndev, ndev);
}

int zx279133_xdp_enqueue(struct zx279133_eth *eth, struct xdp_frame *xdpf)
{
	struct zx279133_tx_ring *tx = &eth->tx[0];
	struct zx279133_idm_desc *desc;
	struct zx279133_tx_slot *slot;
	dma_addr_t dma;
	u16 producer;
	bool arm_reclaim;
	bool dma_mapped;

	if (unlikely(xdp_frame_has_frags(xdpf) || xdpf->len < ETH_HLEN ||
		     xdpf->len > ZX279133_IDM_RX_FRAME_LIMIT))
		return -EOPNOTSUPP;
	if (unlikely(!READ_ONCE(eth->tx_prepared) ||
		     READ_ONCE(eth->tx_stopping)))
		return -ENETDOWN;

	if (xdpf->mem_type == MEM_TYPE_PAGE_POOL &&
	    (netmem_get_pp(virt_to_netmem(xdpf->data)) == eth->rx_page_pool ||
	     netmem_get_pp(virt_to_netmem(xdpf->data)) == eth->lan_rx_page_pool)) {
		struct page *page = virt_to_page(xdpf->data);
		dma_addr_t page_dma = page_pool_get_dma_addr(page);
		unsigned int offset = offset_in_page(xdpf->data);

		dma_sync_single_range_for_device(eth->dev, page_dma, offset,
						 xdpf->len, DMA_BIDIRECTIONAL);
		dma = page_dma + offset;
		dma_mapped = false;
	} else {
		dma = dma_map_single(eth->dev, xdpf->data, xdpf->len,
				     DMA_TO_DEVICE);
		if (dma_mapping_error(eth->dev, dma))
			return -ENOMEM;
		dma_mapped = true;
	}

	spin_lock_bh(&eth->tx_lock);
	if (unlikely(tx->pending >= ZX279133_IDM_TX_DEPTH - 1))
		zx279133_idm_tx_reclaim_queue_locked(eth, 0);
	if (unlikely(tx->pending >= ZX279133_IDM_TX_DEPTH - 1)) {
		spin_unlock_bh(&eth->tx_lock);
		if (dma_mapped)
			dma_unmap_single(eth->dev, dma, xdpf->len,
					 DMA_TO_DEVICE);
		return -ENOSPC;
	}

	producer = tx->producer;
	desc = eth->tx_descs + ZX279133_IDM_CPU_TX_FIRST *
		ZX279133_IDM_TX_DEPTH + producer;
	slot = &tx->slots[producer];
	memset(desc, 0, sizeof(*desc));
	desc->address = cpu_to_le32(lower_32_bits(dma));
	desc->length_flags = cpu_to_le32((xdpf->len << 1) |
					 (zx279133_tx_selector & 0xff) << 24 |
					 (zx279133_tx_word4_bit23 ? BIT(23) : 0));
	desc->metadata[0] = cpu_to_le32(ZX279133_IDM_TX_CONTROL |
					       (zx279133_tx_port & 0xff) << 16);
	desc->metadata[4] = cpu_to_le32(zx279133_tx_pon_control);
	slot->xdpf = xdpf;
	slot->ndev = eth->ndev;
	slot->dma = dma;
	slot->len = xdpf->len;
	slot->dma_mapped = dma_mapped;
	arm_reclaim = !tx->pending;
	tx->producer = (tx->producer + 1) &
				   (ZX279133_IDM_TX_DEPTH - 1);
	tx->pending++;
	tx->notify_pending++;
	spin_unlock_bh(&eth->tx_lock);

	if (arm_reclaim)
		mod_delayed_work(system_wq, &eth->tx_reclaim_work,
				 msecs_to_jiffies(ZX279133_TX_RECLAIM_DELAY_MS));

	return 0;
}

int zx279133_xsk_rx_enqueue(struct zx279133_eth *eth, struct xdp_buff *xdp)
{
	struct zx279133_tx_ring *tx = &eth->tx[0];
	struct zx279133_idm_desc *desc;
	struct zx279133_tx_slot *slot;
	void *dma_data = xdp->data_hard_start + XDP_PACKET_HEADROOM;
	dma_addr_t dma;
	long dma_offset;
	u32 len;
	u16 producer;
	bool arm_reclaim;

	len = xdp->data_end - xdp->data;
	if (unlikely(xdp_buff_has_frags(xdp) || len < ETH_HLEN ||
		     len > ZX279133_IDM_RX_FRAME_LIMIT))
		return -EOPNOTSUPP;
	if (unlikely(!READ_ONCE(eth->tx_prepared) ||
		     READ_ONCE(eth->tx_stopping)))
		return -ENETDOWN;

	dma_offset = xdp->data - dma_data;
	dma = xsk_buff_xdp_get_dma(xdp) + dma_offset;
	if (unlikely(upper_32_bits(dma)))
		return -ERANGE;
	xsk_buff_raw_dma_sync_for_device(eth->xsk_pool, dma, len);

	spin_lock_bh(&eth->tx_lock);
	if (unlikely(tx->pending >= ZX279133_IDM_TX_DEPTH - 1))
		zx279133_idm_tx_reclaim_queue_locked(eth, 0);
	if (unlikely(tx->pending >= ZX279133_IDM_TX_DEPTH - 1)) {
		spin_unlock_bh(&eth->tx_lock);
		return -ENOSPC;
	}

	producer = tx->producer;
	desc = eth->tx_descs + ZX279133_IDM_CPU_TX_FIRST *
		ZX279133_IDM_TX_DEPTH + producer;
	slot = &tx->slots[producer];
	memset(desc, 0, sizeof(*desc));
	desc->address = cpu_to_le32(lower_32_bits(dma));
	desc->length_flags = cpu_to_le32((len << 1) |
					 (zx279133_tx_selector & 0xff) << 24 |
					 (zx279133_tx_word4_bit23 ? BIT(23) : 0));
	desc->metadata[0] = cpu_to_le32(ZX279133_IDM_TX_CONTROL |
					       (zx279133_tx_port & 0xff) << 16);
	desc->metadata[4] = cpu_to_le32(zx279133_tx_pon_control);
	slot->xsk_rx = xdp;
	slot->ndev = eth->ndev;
	slot->dma = dma;
	slot->len = len;
	arm_reclaim = !tx->pending;
	tx->producer = (tx->producer + 1) &
				   (ZX279133_IDM_TX_DEPTH - 1);
	tx->pending++;
	tx->notify_pending++;
	spin_unlock_bh(&eth->tx_lock);

	if (arm_reclaim)
		mod_delayed_work(system_wq, &eth->tx_reclaim_work,
				 msecs_to_jiffies(ZX279133_TX_RECLAIM_DELAY_MS));

	return 0;
}

void zx279133_xsk_tx(struct zx279133_eth *eth)
{
	struct zx279133_tx_ring *tx = &eth->tx[0];
	struct xsk_buff_pool *pool = READ_ONCE(eth->xsk_pool);
	unsigned int dropped = 0;
	bool ring_full = false;
	bool queued = false;

	if (!pool || !READ_ONCE(eth->tx_prepared) ||
	    READ_ONCE(eth->tx_stopping))
		return;

	spin_lock_bh(&eth->tx_lock);
	zx279133_idm_tx_reclaim_queue_locked(eth, 0);
	while (tx->pending < ZX279133_IDM_TX_DEPTH - 1) {
		struct zx279133_idm_desc *desc;
		struct zx279133_tx_slot *slot;
		struct xdp_desc xdp_desc;
		dma_addr_t dma;
		u16 producer;

		if (!xsk_tx_peek_desc(pool, &xdp_desc))
			break;
		if (unlikely(!xsk_is_eop_desc(&xdp_desc) ||
			     xdp_desc.len < ETH_HLEN ||
			     xdp_desc.len > ZX279133_IDM_RX_FRAME_LIMIT)) {
			xsk_tx_release(pool);
			dropped++;
			continue;
		}

		dma = xsk_buff_raw_get_dma(pool, xdp_desc.addr);
		if (unlikely(upper_32_bits(dma))) {
			xsk_tx_release(pool);
			dropped++;
			continue;
		}
		xsk_buff_raw_dma_sync_for_device(pool, dma, xdp_desc.len);

		producer = tx->producer;
		desc = eth->tx_descs + ZX279133_IDM_CPU_TX_FIRST *
			ZX279133_IDM_TX_DEPTH + producer;
		slot = &tx->slots[producer];
		memset(desc, 0, sizeof(*desc));
		desc->address = cpu_to_le32(lower_32_bits(dma));
		desc->length_flags =
			cpu_to_le32((xdp_desc.len << 1) |
				    (zx279133_tx_selector & 0xff) << 24 |
				    (zx279133_tx_word4_bit23 ? BIT(23) : 0));
		desc->metadata[0] =
			cpu_to_le32(ZX279133_IDM_TX_CONTROL |
				    (zx279133_tx_port & 0xff) << 16);
		desc->metadata[4] = cpu_to_le32(zx279133_tx_pon_control);
		slot->ndev = eth->ndev;
		slot->dma = dma;
		slot->len = xdp_desc.len;
		slot->xsk_tx = true;
		tx->producer = (tx->producer + 1) &
					   (ZX279133_IDM_TX_DEPTH - 1);
		tx->pending++;
		tx->notify_pending++;
		xsk_tx_release(pool);
		queued = true;
	}
	ring_full = tx->pending >= ZX279133_IDM_TX_DEPTH - 1;
	if (queued)
		zx279133_idm_tx_flush_queue_locked(eth, 0);
	spin_unlock_bh(&eth->tx_lock);

	if (dropped) {
		xsk_tx_completed(pool, dropped);
		while (dropped--)
			zx279133_stats_tx_dropped(eth, eth->ndev);
	}
	if (xsk_uses_need_wakeup(pool)) {
		if (ring_full)
			xsk_clear_tx_need_wakeup(pool);
		else
			xsk_set_tx_need_wakeup(pool);
	}
	if (queued)
		mod_delayed_work(system_wq, &eth->tx_reclaim_work,
				 msecs_to_jiffies(ZX279133_TX_RECLAIM_DELAY_MS));
}

void zx279133_xdp_flush(struct zx279133_eth *eth)
{
	spin_lock_bh(&eth->tx_lock);
	zx279133_idm_tx_flush_locked(eth);
	spin_unlock_bh(&eth->tx_lock);
}

static int zx279133_xdp_xmit(struct net_device *ndev, int n,
			     struct xdp_frame **frames, u32 flags)
{
	struct zx279133_eth *eth = netdev_priv(ndev);
	int nxmit = 0;
	int ret = 0;

	if (unlikely(flags & ~XDP_XMIT_FLAGS_MASK))
		return -EINVAL;
	if (unlikely(!netif_running(ndev)))
		return -ENETDOWN;

	while (nxmit < n) {
		ret = zx279133_xdp_enqueue(eth, frames[nxmit]);
		if (ret)
			break;
		nxmit++;
	}
	if (flags & XDP_XMIT_FLUSH)
		zx279133_xdp_flush(eth);

	return nxmit ?: ret;
}

/* Caller holds datapath_lock across suspend, configuration and resume. */
static void zx279133_datapath_suspend(struct zx279133_eth *eth)
{
	bool tx_quiesced;

	netif_device_detach(eth->ndev);
	if (eth->lan_ndev)
		netif_device_detach(eth->lan_ndev);
	zx279133_shared_rx_stop(eth);
	tx_quiesced = zx279133_shared_tx_pause(eth);
	if (eth->datapath_users & ZX279133_DATAPATH_USER_WAN)
		phylink_stop(eth->phylink);
	zx279133_shared_idm_release(eth, eth->ndev, tx_quiesced);
}

static int zx279133_datapath_resume(struct zx279133_eth *eth, bool require_hash)
{
	int ret;

	ret = zx279133_hardware_prepare(eth);
	if (!ret && require_hash && !eth->rx_hash_active)
		ret = -EOPNOTSUPP;
	if (!ret)
		ret = zx279133_shared_idm_prepare(eth);
	if (ret) {
		if (READ_ONCE(eth->hardware_prepared))
			zx279133_hardware_unprepare(eth);
		return ret;
	}
	if (eth->datapath_users & ZX279133_DATAPATH_USER_WAN)
		phylink_start(eth->phylink);
	zx279133_shared_rx_start(eth);
	zx279133_shared_tx_resume(eth);
	netif_device_attach(eth->ndev);
	if (eth->lan_ndev)
		netif_device_attach(eth->lan_ndev);
	return 0;
}

static int zx279133_xsk_pool_cycle(struct zx279133_eth *eth,
				   struct xsk_buff_pool *pool)
{
	struct xsk_buff_pool *old_pool = READ_ONCE(eth->xsk_pool);
	bool idle = false;
	int restore;
	int ret;

	if (pool == old_pool)
		return 0;
	if (pool && xsk_pool_get_rx_frame_size(pool) <
		    eth->ndev->mtu + ETH_HLEN + VLAN_HLEN)
		return -EINVAL;
	if (pool) {
		xsk_pool_set_rxq_info(pool, &eth->xsk_rxq);
		ret = xsk_pool_dma_map(pool, eth->dev, DMA_ATTR_SKIP_CPU_SYNC);
		if (ret)
			return ret;
	}

	mutex_lock(&eth->datapath_lock);
	if (!eth->datapath_users) {
		WRITE_ONCE(eth->xsk_pool, pool);
		idle = true;
		mutex_unlock(&eth->datapath_lock);
		goto out_unmap_old;
	}

	zx279133_datapath_suspend(eth);
	WRITE_ONCE(eth->xsk_pool, pool);
	ret = zx279133_datapath_resume(eth, false);
	if (!ret) {
		mutex_unlock(&eth->datapath_lock);
		goto out_unmap_old;
	}

	WRITE_ONCE(eth->xsk_pool, old_pool);
	restore = zx279133_datapath_resume(eth, false);
	if (restore) {
		netdev_err(eth->ndev,
			   "failed to restore datapath after XSK pool setup: %d\n",
			   restore);
	}
	mutex_unlock(&eth->datapath_lock);
	if (pool)
		xsk_pool_dma_unmap(pool, DMA_ATTR_SKIP_CPU_SYNC);
	return ret;

out_unmap_old:
	if (old_pool)
		xsk_pool_dma_unmap(old_pool, DMA_ATTR_SKIP_CPU_SYNC);
	if (pool && xsk_uses_need_wakeup(pool)) {
		if (idle)
			xsk_set_rx_need_wakeup(pool);
		xsk_set_tx_need_wakeup(pool);
	}
	return 0;
}

static int zx279133_xdp(struct net_device *ndev, struct netdev_bpf *bpf)
{
	struct zx279133_eth *eth = netdev_priv(ndev);
	struct bpf_prog *old_prog;

	switch (bpf->command) {
	case XDP_SETUP_PROG:
		break;
	case XDP_SETUP_XSK_POOL:
		if (bpf->xsk.queue_id)
			return -EINVAL;
		return zx279133_xsk_pool_cycle(eth, bpf->xsk.pool);
	default:
		return -EOPNOTSUPP;
	}

	old_prog = rcu_replace_pointer(eth->xdp_prog, bpf->prog,
				       lockdep_rtnl_is_held());
	if (old_prog)
		bpf_prog_put(old_prog);

	return 0;
}

static int zx279133_xsk_wakeup(struct net_device *ndev, u32 queue_id,
			       u32 flags)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	if (queue_id || !READ_ONCE(eth->xsk_pool))
		return -EINVAL;
	if (!READ_ONCE(eth->rx_running) || !READ_ONCE(eth->napi_enabled))
		return -ENETDOWN;
	if (!napi_if_scheduled_mark_missed(&eth->napi))
		napi_schedule(&eth->napi);

	return 0;
}

static void zx279133_tx_timeout_common(struct zx279133_eth *eth,
				       struct net_device *ndev,
				       unsigned int txqueue)
{
	struct zx279133_tx_ring *tx;
	u16 done_before = 0, done_after = 0;
	u16 pending_before = 0, pending_after = 0;
	u16 producer = 0, consumer = 0, notify = 0;
	unsigned int reclaimed = 0;
	u32 int_mask = 0;
	bool prepared, stopping;

	if (txqueue >= ZX279133_CPU_TX_QUEUES)
		return;
	tx = &eth->tx[txqueue];

	spin_lock_bh(&eth->tx_lock);
	if (!tx->pending) {
		spin_unlock_bh(&eth->tx_lock);
		return;
	}
	eth->tx_timeouts++;
	prepared = READ_ONCE(eth->hardware_prepared);
	stopping = READ_ONCE(eth->tx_stopping);
	if (prepared && !stopping) {
		void __iomem *idm = eth->base + ZX279133_IDM_BASE;

		done_before = readl(idm +
				    zx279133_idm_tx_done_reg(ZX279133_IDM_CPU_TX_FIRST + txqueue)) &
			      0xffff;
		pending_before = tx->pending;
		producer = tx->producer;
		consumer = tx->consumer;
		notify = tx->notify_pending;
		int_mask = readl(idm + ZX279133_IDM_INT_MASK);
		zx279133_idm_tx_flush_queue_locked(eth, txqueue);
		reclaimed = zx279133_idm_tx_reclaim_queue_locked(eth, txqueue);
		done_after = tx->done;
		pending_after = tx->pending;
		if (reclaimed)
			eth->tx_timeout_recoveries++;
		else if (pending_after)
			eth->tx_timeout_stalls++;
	}
	spin_unlock_bh(&eth->tx_lock);

	zx279133_stats_tx_error(eth, ndev);
	/* Both logical devices share each physical ring. Keep either watchdog
	 * from immediately retriggering after the common recovery attempt.
	 */
	txq_trans_update(eth->ndev, netdev_get_tx_queue(eth->ndev, txqueue));
	if (eth->lan_ndev)
		txq_trans_update(eth->lan_ndev,
				 netdev_get_tx_queue(eth->lan_ndev, txqueue));

	if (!prepared || stopping) {
		netdev_warn(ndev,
			    "TX timeout while datapath unavailable (prepared=%u stopping=%u)\n",
			    prepared, stopping);
		return;
	}

	if (reclaimed || !pending_after) {
		netdev_warn(ndev,
			    "TX timeout recovered: reclaimed=%u pending=%u->%u done=%#x->%#x prod=%u cons=%u notify=%u mask=%#x\n",
			    reclaimed, pending_before, pending_after,
			    done_before, done_after, producer, consumer, notify,
			    int_mask);
		return;
	}

	netdev_err(ndev,
		   "TX timeout stalled: pending=%u done=%#x prod=%u cons=%u notify=%u mask=%#x\n",
		   pending_after, done_after, producer, consumer, notify,
		   int_mask);
}

static void zx279133_tx_timeout(struct net_device *ndev,
				unsigned int txqueue)
{
	zx279133_tx_timeout_common(netdev_priv(ndev), ndev, txqueue);
}

static int zx279133_set_mac_address(struct net_device *ndev, void *p)
{
	struct zx279133_eth *eth = netdev_priv(ndev);
	struct sockaddr *addr = p;
	int ret;

	ret = eth_prepare_mac_addr_change(ndev, p);
	if (ret)
		return ret;
	if (netif_running(ndev) && READ_ONCE(eth->hardware_prepared)) {
		zx279133_program_spa_cpu_mac(eth, addr->sa_data);
		ret = zx279133_program_wanid_cpu_mac(eth, addr->sa_data);
		if (ret)
			return ret;
	}
	eth_commit_mac_addr_change(ndev, p);
	return 0;
}

static int zx279133_change_mtu(struct net_device *ndev, int new_mtu)
{
	struct zx279133_eth *eth = netdev_priv(ndev);
	struct xsk_buff_pool *pool = READ_ONCE(eth->xsk_pool);

	if (pool && xsk_pool_get_rx_frame_size(pool) <
		    new_mtu + ETH_HLEN + VLAN_HLEN)
		return -EINVAL;
	WRITE_ONCE(ndev->mtu, new_mtu);
	return 0;
}

static void
zx279133_fill_stats64(struct net_device *ndev,
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

static void zx279133_get_stats64(struct net_device *ndev,
				 struct rtnl_link_stats64 *stats)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	zx279133_fill_stats64(ndev, &eth->stats, stats);
}

static void zx279133_get_drvinfo(struct net_device *ndev,
				 struct ethtool_drvinfo *info)
{
	strscpy(info->driver, KBUILD_MODNAME, sizeof(info->driver));
	strscpy(info->bus_info, dev_name(ndev->dev.parent),
		sizeof(info->bus_info));
}

static int zx279133_get_link_ksettings(struct net_device *ndev,
				       struct ethtool_link_ksettings *cmd)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	return phylink_ethtool_ksettings_get(eth->phylink, cmd);
}

static int zx279133_set_link_ksettings(struct net_device *ndev,
				       const struct ethtool_link_ksettings *cmd)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	return phylink_ethtool_ksettings_set(eth->phylink, cmd);
}

static int zx279133_nway_reset(struct net_device *ndev)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	return phylink_ethtool_nway_reset(eth->phylink);
}

static void zx279133_get_pauseparam(struct net_device *ndev,
				    struct ethtool_pauseparam *pause)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	phylink_ethtool_get_pauseparam(eth->phylink, pause);
}

static int zx279133_set_pauseparam(struct net_device *ndev,
				   struct ethtool_pauseparam *pause)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	return phylink_ethtool_set_pauseparam(eth->phylink, pause);
}

enum zx279133_ethtool_stat {
	ZX279133_STAT_RX_PACKETS,
	ZX279133_STAT_TX_PACKETS,
	ZX279133_STAT_RX_BYTES,
	ZX279133_STAT_TX_BYTES,
	ZX279133_STAT_RX_ERRORS,
	ZX279133_STAT_TX_ERRORS,
	ZX279133_STAT_RX_DROPPED,
	ZX279133_STAT_TX_DROPPED,
	ZX279133_STAT_RX_LENGTH,
	ZX279133_STAT_XMAC_TX_FRAMES,
	ZX279133_STAT_XMAC_TX_GOOD,
	ZX279133_STAT_SOPC_READY,
	ZX279133_STAT_SOPC_SEND,
	ZX279133_STAT_NPPT_XMAC_ERR,
	ZX279133_STAT_SMCT_DONE,
	ZX279133_STAT_SSCH5,
	ZX279133_STAT_SOPC_RR5,
	ZX279133_STAT_SOPC_TO_SMAC5,
	ZX279133_STAT_SOPC_REQ_SMAC5,
	ZX279133_STAT_IDM_TX_DONE,
	ZX279133_STAT_TX_PENDING,
	ZX279133_STAT_TX_DOORBELL_WRITES,
	ZX279133_STAT_TX_DOORBELL_DESCS,
	ZX279133_STAT_TX_RECLAIM_POLLS,
	ZX279133_STAT_TX_RECLAIM_WORK_RUNS,
	ZX279133_STAT_TX_RECLAIM_WORK_PACKETS,
	ZX279133_STAT_TX_TIMEOUTS,
	ZX279133_STAT_TX_TIMEOUT_RECOVERIES,
	ZX279133_STAT_TX_TIMEOUT_STALLS,
	ZX279133_STAT_TX0_SUBMITTED,
	ZX279133_STAT_TX0_COMPLETED,
	ZX279133_STAT_TX1_SUBMITTED,
	ZX279133_STAT_TX1_COMPLETED,
	ZX279133_STAT_TX_HW_CSUM_PACKETS,
	ZX279133_STAT_TX_SW_CSUM_PACKETS,
	ZX279133_STAT_RX_IRQ_COUNT,
	ZX279133_STAT_IDM_LOCAL_IRQ_COUNT,
	ZX279133_STAT_RX_NAPI_POLLS,
	ZX279133_STAT_RX_NAPI_WORK,
	ZX279133_STAT_RX_NAPI_BUDGET_EXHAUSTIONS,
	ZX279133_STAT_RX_DESC_NOT_READY,
	ZX279133_STAT_RX_INVALID_DMA,
	ZX279133_STAT_RX_PAGE_LOOKUP_MISSES,
	ZX279133_STAT_RX_JUMBO_DROPS,
	ZX279133_STAT_RX_DESCRIPTOR_FLAG_DROPS,
	ZX279133_STAT_RX_PAGE_ALLOC_FAILURES,
	ZX279133_STAT_RX_COPY_FALLBACKS,
	ZX279133_STAT_RX_SKB_ALLOC_FAILURES,
	ZX279133_STAT_RX_HW_CSUM_PACKETS,
	ZX279133_STAT_RX_HW_HASH_PACKETS,
	ZX279133_STAT_RX_REFILL_POST_FAILURES,
	ZX279133_STAT_RX_REFILL_SHORTFALLS,
	ZX279133_STAT_RX_REFILL_PUBLISHED,
	ZX279133_STAT_RX_RELEASE_PUBLISHED,
	ZX279133_STAT_RX_PAGE_MAP_COUNT,
	ZX279133_STAT_RX_PAGE_MAP_HIGH_WATER,
	ZX279133_STAT_RX_REFILL_DEFICIT,
	ZX279133_STAT_RX_REFILL_DEFICIT_HIGH_WATER,
	ZX279133_STAT_RX_REFILL_RECOVERY_ATTEMPTS,
	ZX279133_STAT_RX_REFILL_RECOVERY_PAGES,
	ZX279133_STAT_RX_REFILL_RECOVERY_FAILURES,
	ZX279133_STAT_RX_REFILL_RETRY_WORK_RUNS,
	ZX279133_STAT_XDP_PASS,
	ZX279133_STAT_XDP_DROP,
	ZX279133_STAT_XDP_TX,
	ZX279133_STAT_XDP_REDIRECT,
	ZX279133_STAT_XDP_ABORTED,
	ZX279133_STAT_COUNT,
};

static const char zx279133_gstrings_stats[ZX279133_STAT_COUNT][ETH_GSTRING_LEN] = {
	"rx_packets",
	"tx_packets",
	"rx_bytes",
	"tx_bytes",
	"rx_errors",
	"tx_errors",
	"rx_dropped",
	"tx_dropped",
	"rx_length_errors",
	"xmac_tx_frames",
	"xmac_tx_good_frames",
	"sopc_ready",
	"sopc_send",
	"nppt_xmac_err",
	"smct_done",
	"ssch5",
	"sopc_rr5",
	"sopc_to_smac5",
	"sopc_req_smac5",
	"idm_tx_done",
	"tx_pending",
	"tx_doorbell_writes",
	"tx_doorbell_descs",
	"tx_reclaim_polls",
	"tx_reclaim_work_runs",
	"tx_reclaim_work_packets",
	"tx_timeouts",
	"tx_timeout_recoveries",
	"tx_timeout_stalls",
	"tx_queue_0_submitted",
	"tx_queue_0_completed",
	"tx_queue_1_submitted",
	"tx_queue_1_completed",
	"tx_hw_csum_packets",
	"tx_sw_csum_packets",
	"rx_irq_count",
	"idm_local_irq_count",
	"rx_napi_polls",
	"rx_napi_work",
	"rx_napi_budget_exhaustions",
	"rx_desc_not_ready",
	"rx_invalid_dma",
	"rx_page_lookup_misses",
	"rx_jumbo_drops",
	"rx_descriptor_flag_drops",
	"rx_page_alloc_failures",
	"rx_copy_fallbacks",
	"rx_skb_alloc_failures",
	"rx_hw_csum_packets",
	"rx_hw_hash_packets",
	"rx_refill_post_failures",
	"rx_refill_shortfalls",
	"rx_refill_published",
	"rx_release_published",
	"rx_page_map_count",
	"rx_page_map_high_water",
	"rx_refill_deficit",
	"rx_refill_deficit_high_water",
	"rx_refill_recovery_attempts",
	"rx_refill_recovery_pages",
	"rx_refill_recovery_failures",
	"rx_refill_retry_work_runs",
	"xdp_pass",
	"xdp_drop",
	"xdp_tx",
	"xdp_redirect",
	"xdp_aborted",
};

static u32 zx279133_hw_stat(struct zx279133_eth *eth, unsigned int offset)
{
	if (!READ_ONCE(eth->hardware_prepared))
		return 0;
	return readl(eth->base + offset);
}

static int zx279133_get_sset_count(struct net_device *ndev, int sset)
{
	if (sset == ETH_SS_STATS)
		return ZX279133_STAT_COUNT + ZX279133_IDM_CPU_RX_QUEUES + 16;
	return -EOPNOTSUPP;
}

static void zx279133_get_strings(struct net_device *ndev, u32 sset, u8 *data)
{
	unsigned int i;

	if (sset != ETH_SS_STATS)
		return;
	memcpy(data, zx279133_gstrings_stats, sizeof(zx279133_gstrings_stats));
	data += sizeof(zx279133_gstrings_stats);
	for (i = 0; i < ZX279133_IDM_CPU_RX_QUEUES; i++)
		ethtool_sprintf(&data, "rx_hw_queue_%u_descs", i);
	for (i = 0; i < 2; i++) {
		ethtool_sprintf(&data, "rx_napi_%u_polls", i);
		ethtool_sprintf(&data, "rx_napi_%u_cpu0", i);
		ethtool_sprintf(&data, "rx_napi_%u_cpu1", i);
		ethtool_sprintf(&data, "rx_napi_%u_overlaps", i);
		ethtool_sprintf(&data, "rx_napi_%u_foreign_pages", i);
		ethtool_sprintf(&data, "rx_napi_%u_lan_descs", i);
		ethtool_sprintf(&data, "rx_napi_%u_wan_descs", i);
	}
	ethtool_puts(&data, "rx_hash_active");
	ethtool_puts(&data, "rx_hash_loads");
}

static void zx279133_get_ethtool_stats(struct net_device *ndev,
				       struct ethtool_stats *stats, u64 *data)
{
	struct rtnl_link_stats64 sw_stats = {};
	struct zx279133_eth *eth = netdev_priv(ndev);
	u64 rx_data[ZX279133_STAT_COUNT + ZX279133_IDM_CPU_RX_QUEUES] = {};
	unsigned int group;
	unsigned int start;
	unsigned int i;

	zx279133_fill_stats64(ndev, &eth->stats, &sw_stats);
	data[ZX279133_STAT_RX_PACKETS] = sw_stats.rx_packets;
	data[ZX279133_STAT_TX_PACKETS] = sw_stats.tx_packets;
	data[ZX279133_STAT_RX_BYTES] = sw_stats.rx_bytes;
	data[ZX279133_STAT_TX_BYTES] = sw_stats.tx_bytes;
	data[ZX279133_STAT_RX_ERRORS] = sw_stats.rx_errors;
	data[ZX279133_STAT_TX_ERRORS] = sw_stats.tx_errors;
	data[ZX279133_STAT_RX_DROPPED] = sw_stats.rx_dropped;
	data[ZX279133_STAT_TX_DROPPED] = sw_stats.tx_dropped;
	data[ZX279133_STAT_RX_LENGTH] = sw_stats.rx_length_errors;
	data[ZX279133_STAT_XMAC_TX_FRAMES] =
		zx279133_hw_stat(eth, ZX279133_XMAC1_BASE +
				 ZX279133_XMAC_STAT_TX_FRAMES);
	data[ZX279133_STAT_XMAC_TX_GOOD] =
		zx279133_hw_stat(eth, ZX279133_XMAC1_BASE +
				 ZX279133_XMAC_STAT_TX_GOOD);
	data[ZX279133_STAT_SOPC_READY] =
		zx279133_hw_stat(eth, ZX279133_XMAC1_SOPC_READY);
	data[ZX279133_STAT_SOPC_SEND] =
		zx279133_hw_stat(eth, ZX279133_XMAC1_SOPC_SEND_ENABLE);
	data[ZX279133_STAT_NPPT_XMAC_ERR] =
		zx279133_hw_stat(eth, ZX279133_NPPT_XMAC_ERR);
	data[ZX279133_STAT_SMCT_DONE] =
		zx279133_hw_stat(eth, ZX279133_SMCT_DONE);
	data[ZX279133_STAT_SSCH5] = zx279133_hw_stat(eth, ZX279133_SSCH5);
	data[ZX279133_STAT_SOPC_RR5] = zx279133_hw_stat(eth, ZX279133_SOPC_RR5);
	data[ZX279133_STAT_SOPC_TO_SMAC5] =
		zx279133_hw_stat(eth, ZX279133_SOPC_TO_SMAC5);
	data[ZX279133_STAT_SOPC_REQ_SMAC5] =
		zx279133_hw_stat(eth, ZX279133_SOPC_REQ_SMAC5);
	spin_lock_bh(&eth->tx_lock);
	data[ZX279133_STAT_IDM_TX_DONE] =
		eth->tx_prepared && READ_ONCE(eth->hardware_prepared) ?
		readl(eth->base + ZX279133_IDM_BASE +
		      zx279133_idm_tx_done_reg(ZX279133_IDM_CPU_TX_FIRST)) & 0xffff : 0;
	data[ZX279133_STAT_TX_PENDING] = zx279133_idm_tx_pending(eth);
	data[ZX279133_STAT_TX_DOORBELL_WRITES] = eth->tx_doorbell_writes;
	data[ZX279133_STAT_TX_DOORBELL_DESCS] = eth->tx_doorbell_descs;
	data[ZX279133_STAT_TX_RECLAIM_POLLS] = eth->tx_reclaim_polls;
	data[ZX279133_STAT_TX_RECLAIM_WORK_RUNS] =
		eth->tx_reclaim_work_runs;
	data[ZX279133_STAT_TX_RECLAIM_WORK_PACKETS] =
		eth->tx_reclaim_work_packets;
	data[ZX279133_STAT_TX_TIMEOUTS] = eth->tx_timeouts;
	data[ZX279133_STAT_TX_TIMEOUT_RECOVERIES] =
		eth->tx_timeout_recoveries;
	data[ZX279133_STAT_TX_TIMEOUT_STALLS] = eth->tx_timeout_stalls;
	data[ZX279133_STAT_TX0_SUBMITTED] = eth->tx[0].submitted;
	data[ZX279133_STAT_TX0_COMPLETED] = eth->tx[0].completed;
	data[ZX279133_STAT_TX1_SUBMITTED] = eth->tx[1].submitted;
	data[ZX279133_STAT_TX1_COMPLETED] = eth->tx[1].completed;
	data[ZX279133_STAT_TX_HW_CSUM_PACKETS] = eth->tx_hw_csum_packets;
	data[ZX279133_STAT_TX_SW_CSUM_PACKETS] = eth->tx_sw_csum_packets;
	spin_unlock_bh(&eth->tx_lock);
	data[ZX279133_STAT_RX_IRQ_COUNT] = atomic64_read(&eth->rx_irq_count);
	data[ZX279133_STAT_IDM_LOCAL_IRQ_COUNT] =
		atomic64_read(&eth->idm_local_irq_count);
	memset(data + ZX279133_STAT_RX_NAPI_POLLS, 0,
	       sizeof(u64) * (ZX279133_STAT_COUNT + ZX279133_IDM_CPU_RX_QUEUES -
			      ZX279133_STAT_RX_NAPI_POLLS));
	for (group = 0; group < 2; group++) {
		struct zx279133_rx_stats *stats = &eth->rx_stats[group];

		do {
			start = u64_stats_fetch_begin(&stats->syncp);
			rx_data[ZX279133_STAT_RX_NAPI_POLLS] =
				u64_stats_read(&stats->rx_napi_polls);
			rx_data[ZX279133_STAT_RX_NAPI_WORK] =
				u64_stats_read(&stats->rx_napi_work);
			for (i = 0; i < ZX279133_IDM_CPU_RX_QUEUES; i++)
				rx_data[ZX279133_STAT_COUNT + i] =
					u64_stats_read(&stats->rx_queue_descs[i]);
			rx_data[ZX279133_STAT_RX_NAPI_BUDGET_EXHAUSTIONS] =
				u64_stats_read(&stats->rx_napi_budget_exhaustions);
			rx_data[ZX279133_STAT_RX_DESC_NOT_READY] =
				u64_stats_read(&stats->rx_desc_not_ready);
			rx_data[ZX279133_STAT_RX_INVALID_DMA] =
				u64_stats_read(&stats->rx_invalid_dma);
			rx_data[ZX279133_STAT_RX_PAGE_LOOKUP_MISSES] =
				u64_stats_read(&stats->rx_page_lookup_misses);
			rx_data[ZX279133_STAT_RX_JUMBO_DROPS] =
				u64_stats_read(&stats->rx_jumbo_drops);
			rx_data[ZX279133_STAT_RX_DESCRIPTOR_FLAG_DROPS] =
				u64_stats_read(&stats->rx_descriptor_flag_drops);
			rx_data[ZX279133_STAT_RX_PAGE_ALLOC_FAILURES] =
				u64_stats_read(&stats->rx_page_alloc_failures);
			rx_data[ZX279133_STAT_RX_COPY_FALLBACKS] =
				u64_stats_read(&stats->rx_copy_fallbacks);
			rx_data[ZX279133_STAT_RX_SKB_ALLOC_FAILURES] =
				u64_stats_read(&stats->rx_skb_alloc_failures);
			rx_data[ZX279133_STAT_RX_HW_CSUM_PACKETS] =
				u64_stats_read(&stats->rx_hw_csum_packets);
			rx_data[ZX279133_STAT_RX_HW_HASH_PACKETS] =
				u64_stats_read(&stats->rx_hw_hash_packets);
			rx_data[ZX279133_STAT_RX_REFILL_POST_FAILURES] =
				u64_stats_read(&stats->rx_refill_post_failures);
			rx_data[ZX279133_STAT_RX_REFILL_SHORTFALLS] =
				u64_stats_read(&stats->rx_refill_shortfalls);
			rx_data[ZX279133_STAT_RX_REFILL_PUBLISHED] =
				u64_stats_read(&stats->rx_refill_published);
			rx_data[ZX279133_STAT_RX_RELEASE_PUBLISHED] =
				u64_stats_read(&stats->rx_release_published);
			rx_data[ZX279133_STAT_RX_REFILL_RECOVERY_ATTEMPTS] =
				u64_stats_read(&stats->rx_refill_recovery_attempts);
			rx_data[ZX279133_STAT_RX_REFILL_RECOVERY_PAGES] =
				u64_stats_read(&stats->rx_refill_recovery_pages);
			rx_data[ZX279133_STAT_RX_REFILL_RECOVERY_FAILURES] =
				u64_stats_read(&stats->rx_refill_recovery_failures);
			rx_data[ZX279133_STAT_XDP_PASS] =
				u64_stats_read(&stats->xdp_pass);
			rx_data[ZX279133_STAT_XDP_DROP] =
				u64_stats_read(&stats->xdp_drop);
			rx_data[ZX279133_STAT_XDP_TX] =
				u64_stats_read(&stats->xdp_tx);
			rx_data[ZX279133_STAT_XDP_REDIRECT] =
				u64_stats_read(&stats->xdp_redirect);
			rx_data[ZX279133_STAT_XDP_ABORTED] =
				u64_stats_read(&stats->xdp_aborted);
			rx_data[0] = u64_stats_read(&stats->rx_napi_polls);
			rx_data[1] = u64_stats_read(&stats->cpu_polls[0]);
			rx_data[2] = u64_stats_read(&stats->cpu_polls[1]);
			rx_data[3] = u64_stats_read(&stats->overlaps);
			rx_data[4] = u64_stats_read(&stats->foreign_pages);
			rx_data[5] = 0;
			rx_data[6] = 0;
			for (i = 0; i < ZX279133_IDM_CPU_RX_QUEUES; i++)
				rx_data[5 + (i >= ZX279133_LAN_RX_QUEUE_COUNT)] +=
					u64_stats_read(&stats->rx_queue_descs[i]);
		} while (u64_stats_fetch_retry(&stats->syncp, start));
		for (i = ZX279133_STAT_RX_NAPI_POLLS;
		     i < ZX279133_STAT_COUNT + ZX279133_IDM_CPU_RX_QUEUES; i++)
			data[i] += rx_data[i];
		for (i = 0; i < 7; i++)
			data[ZX279133_STAT_COUNT + ZX279133_IDM_CPU_RX_QUEUES +
			     7 * group + i] = rx_data[i];
	}
	data[ZX279133_STAT_COUNT + ZX279133_IDM_CPU_RX_QUEUES + 14] =
		READ_ONCE(eth->rx_hash_active);
	data[ZX279133_STAT_COUNT + ZX279133_IDM_CPU_RX_QUEUES + 15] =
		READ_ONCE(eth->rx_hash_loads);
	data[ZX279133_STAT_RX_PAGE_MAP_COUNT] =
		READ_ONCE(eth->rx_page_map_count);
	data[ZX279133_STAT_RX_PAGE_MAP_HIGH_WATER] =
		READ_ONCE(eth->rx_page_map_high_water);
	data[ZX279133_STAT_RX_REFILL_DEFICIT] =
		READ_ONCE(eth->rx_refill_deficit);
	data[ZX279133_STAT_RX_REFILL_DEFICIT_HIGH_WATER] =
		READ_ONCE(eth->rx_refill_deficit_high_water);
	data[ZX279133_STAT_RX_REFILL_RETRY_WORK_RUNS] =
		atomic64_read(&eth->rx_refill_retry_work_runs);
}

static void
zx279133_get_ringparam(struct net_device *ndev,
			struct ethtool_ringparam *ring,
			struct kernel_ethtool_ringparam *kernel_ring,
			struct netlink_ext_ack *extack)
{
	ring->rx_max_pending = ZX279133_IDM_RX_RING_SIZE;
	ring->tx_max_pending = ZX279133_IDM_TX_DEPTH;
	ring->rx_pending = ZX279133_IDM_RX_RING_SIZE;
	ring->tx_pending = ZX279133_IDM_TX_DEPTH;
	kernel_ring->rx_buf_len = ZX279133_RX_PAGE_SIZE;
}

static int zx279133_get_eee(struct net_device *ndev, struct ethtool_keee *eee)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	return phylink_ethtool_get_eee(eth->phylink, eee);
}

static int zx279133_set_eee(struct net_device *ndev, struct ethtool_keee *eee)
{
	struct zx279133_eth *eth = netdev_priv(ndev);
	struct ethtool_keee old = { };
	struct ethtool_keee updated = { };
	int ret;

	ret = phylink_ethtool_get_eee(eth->phylink, &old);
	if (ret)
		return ret;
	ret = phylink_ethtool_set_eee(eth->phylink, eee);
	if (ret)
		return ret;
	ret = phylink_ethtool_get_eee(eth->phylink, &updated);
	if (ret)
		return ret;

	if (netif_running(ndev) &&
	    (old.eee_enabled != updated.eee_enabled ||
	     old.tx_lpi_enabled != updated.tx_lpi_enabled ||
	     old.tx_lpi_timer != updated.tx_lpi_timer ||
	     !linkmode_equal(old.advertised, updated.advertised))) {
		/* ZX279051 keeps carrier asserted across an EEE AN restart. */
		phylink_stop(eth->phylink);
		phylink_start(eth->phylink);
	}

	return 0;
}

static int zx279133_get_rxfh_common(struct zx279133_eth *eth, unsigned int port,
				    struct ethtool_rxfh_param *rxfh)
{
	unsigned int i;

	if (rxfh->rss_context)
		return -EOPNOTSUPP;
	mutex_lock(&eth->datapath_lock);
	if (!eth->rx_hash_active) {
		mutex_unlock(&eth->datapath_lock);
		return -EOPNOTSUPP;
	}
	rxfh->hfunc = ETH_RSS_HASH_XOR;
	/* Report Linux delivery queues, including the shared-UMEM fallback. */
	if (rxfh->indir)
		for (i = 0; i < 8; i++)
			rxfh->indir[i] = eth->xsk_pool ? 0 : (eth->rx_rss_map[port] >> i) & 1;
	mutex_unlock(&eth->datapath_lock);
	return 0;
}

static int zx279133_set_rxfh_common(struct zx279133_eth *eth, unsigned int port,
				    struct ethtool_rxfh_param *rxfh,
				    struct netlink_ext_ack *extack)
{
	u8 old, map;
	unsigned int i;
	int ret = 0, restore;

	if (rxfh->rss_context || rxfh->rss_delete || rxfh->key ||
	    (rxfh->input_xfrm && rxfh->input_xfrm != RXH_XFRM_NO_CHANGE) ||
	    (rxfh->hfunc && rxfh->hfunc != ETH_RSS_HASH_XOR)) {
		NL_SET_ERR_MSG_MOD(extack, "Only the keyless XOR default context is supported");
		return -EOPNOTSUPP;
	}
	mutex_lock(&eth->datapath_lock);
	if (!eth->rx_hash_active) {
		NL_SET_ERR_MSG_MOD(extack, "The calibrated RX hash must be active");
		ret = -EOPNOTSUPP;
		goto out;
	}
	if (eth->xsk_pool) {
		NL_SET_ERR_MSG_MOD(extack, "Detach the shared AF_XDP UMEM before changing RSS");
		ret = -EBUSY;
		goto out;
	}
	old = eth->rx_rss_map[port];
	map = old;
	if (rxfh->indir) {
		map = 0;
		for (i = 0; i < 8; i++) {
			if (rxfh->indir[i] >= ZX279133_CPU_RX_QUEUES) {
				ret = -EINVAL;
				goto out;
			}
			map |= rxfh->indir[i] << i;
		}
	}
	if (map == old)
		goto out;
	if (zx279133_flow_offload_active(eth)) {
		NL_SET_ERR_MSG_MOD(extack, "Remove hardware-offloaded flows before changing RSS");
		ret = -EBUSY;
		goto out;
	}

	zx279133_datapath_suspend(eth);
	eth->rx_rss_map[port] = map;
	ret = zx279133_datapath_resume(eth, true);
	if (!ret)
		goto out;
	eth->rx_rss_map[port] = old;
	/* An uncalibrated but valid firmware can still restore native RX. */
	restore = zx279133_datapath_resume(eth, false);
	if (restore)
		netdev_err(eth->ndev, "failed to restore datapath after RSS setup: %d\n",
			   restore);
	else if (!eth->rx_hash_active)
		NL_SET_ERR_MSG_MOD(extack, "RSS setup failed; restored native RX without hashing");
out:
	mutex_unlock(&eth->datapath_lock);
	return ret;
}

static int zx279133_get_rxfh_fields_common(struct zx279133_eth *eth,
					   struct ethtool_rxfh_fields *fields)
{
	if (fields->rss_context)
		return -EOPNOTSUPP;
	fields->data = 0;
	switch (fields->flow_type) {
	case TCP_V6_FLOW:
	case UDP_V6_FLOW:
		if (!READ_ONCE(eth->rx_hash_ipv6_active))
			return 0;
		fallthrough;
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
		if (READ_ONCE(eth->rx_hash_active))
			fields->data = RXH_IP_SRC | RXH_IP_DST | RXH_L3_PROTO |
				       RXH_L4_B_0_1 | RXH_L4_B_2_3;
		return 0;
	case SCTP_V4_FLOW:
	case AH_ESP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case IPV4_FLOW:
	case SCTP_V6_FLOW:
	case AH_ESP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
	case IPV6_FLOW:
	case ETHER_FLOW:
		return 0;
	default:
		return -EINVAL;
	}
}

static u32 zx279133_get_rxfh_indir_size(struct net_device *ndev)
{
	return 8;
}

static int zx279133_get_rxfh(struct net_device *ndev,
			     struct ethtool_rxfh_param *rxfh)
{
	return zx279133_get_rxfh_common(netdev_priv(ndev), 0, rxfh);
}

static int zx279133_set_rxfh(struct net_device *ndev,
			     struct ethtool_rxfh_param *rxfh,
			     struct netlink_ext_ack *extack)
{
	return zx279133_set_rxfh_common(netdev_priv(ndev), 0, rxfh, extack);
}

static int zx279133_get_rxfh_fields(struct net_device *ndev,
				    struct ethtool_rxfh_fields *fields)
{
	return zx279133_get_rxfh_fields_common(netdev_priv(ndev), fields);
}

static int zx279133_get_rxnfc(struct net_device *ndev,
			      struct ethtool_rxnfc *info, u32 *rules)
{
	if (info->cmd != ETHTOOL_GRXRINGS)
		return -EOPNOTSUPP;
	info->data = ndev->real_num_rx_queues;
	return 0;
}

static void zx279133_get_channels(struct net_device *ndev,
				  struct ethtool_channels *channels)
{
	channels->max_rx = ZX279133_CPU_RX_QUEUES;
	channels->max_tx = ZX279133_CPU_TX_QUEUES;
	channels->rx_count = ndev->real_num_rx_queues;
	channels->tx_count = ndev->real_num_tx_queues;
}

const struct ethtool_ops zx279133_ethtool_ops = {
	.get_drvinfo		= zx279133_get_drvinfo,
	.get_link		= ethtool_op_get_link,
	.get_ts_info		= zx279133_get_ts_info,
	.get_link_ksettings	= zx279133_get_link_ksettings,
	.set_link_ksettings	= zx279133_set_link_ksettings,
	.nway_reset		= zx279133_nway_reset,
	.get_pauseparam		= zx279133_get_pauseparam,
	.set_pauseparam		= zx279133_set_pauseparam,
	.get_ringparam		= zx279133_get_ringparam,
	.get_channels		= zx279133_get_channels,
	.get_rxnfc		= zx279133_get_rxnfc,
	.get_rxfh_indir_size	= zx279133_get_rxfh_indir_size,
	.get_rxfh		= zx279133_get_rxfh,
	.set_rxfh		= zx279133_set_rxfh,
	.get_rxfh_fields	= zx279133_get_rxfh_fields,
	.get_eee		= zx279133_get_eee,
	.set_eee		= zx279133_set_eee,
	.get_sset_count		= zx279133_get_sset_count,
	.get_strings		= zx279133_get_strings,
	.get_ethtool_stats	= zx279133_get_ethtool_stats,
};

/*
 * RX admission is owned by the fixed NPPT/PPU port-flow image, not an XMAC
 * address filter. It delivers broadcast, foreign unicast, and arbitrary
 * multicast frames to IDM, so there is no hardware filter for ndo_set_rx_mode
 * to update and IFF_UNICAST_FLT must remain clear.
 */
static int zx279133_setup_tc(struct net_device *ndev,
			    enum tc_setup_type type, void *type_data)
{
	return zx279133_flow_offload_setup_tc(netdev_priv(ndev), ndev, type,
					     type_data);
}

static int zx279133_eth_ioctl(struct net_device *ndev,
			      struct ifreq *ifr, int cmd)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	return phylink_mii_ioctl(eth->phylink, ifr, cmd);
}

const struct net_device_ops zx279133_netdev_ops = {
	.ndo_open		= zx279133_open,
	.ndo_stop		= zx279133_stop,
	.ndo_start_xmit		= zx279133_start_xmit,
	.ndo_tx_timeout		= zx279133_tx_timeout,
	.ndo_set_mac_address	= zx279133_set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= zx279133_change_mtu,
	.ndo_get_stats64	= zx279133_get_stats64,
	.ndo_eth_ioctl		= zx279133_eth_ioctl,
	.ndo_setup_tc		= zx279133_setup_tc,
	.ndo_bpf		= zx279133_xdp,
	.ndo_xdp_xmit		= zx279133_xdp_xmit,
	.ndo_xsk_wakeup		= zx279133_xsk_wakeup,
};

static struct zx279133_eth *
zx279133_lan_service_to_eth(struct zx279133_lan_service *service);

static int
zx279133_lan_netdev_setup(struct zx279133_lan_service *service,
			  struct net_device *ndev)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);
	int ret = 0;

	mutex_lock(&eth->datapath_lock);
	if (eth->lan_ndev && eth->lan_ndev != ndev) {
		ret = -EBUSY;
		goto out_unlock;
	}
	eth->lan_ndev = ndev;
	ndev->watchdog_timeo = eth->ndev->watchdog_timeo;
	ndev->min_mtu = eth->ndev->min_mtu;
	ndev->max_mtu = ZX279133_LAN_MAX_MTU;
	ndev->features = eth->ndev->features;
	ndev->hw_features = eth->ndev->hw_features;
	ndev->vlan_features = eth->ndev->vlan_features;
	eth_hw_addr_set(ndev, eth->ndev->dev_addr);
	netif_carrier_off(ndev);

out_unlock:
	mutex_unlock(&eth->datapath_lock);
	return ret;
}

static void
zx279133_lan_netdev_teardown(struct zx279133_lan_service *service,
			     struct net_device *ndev)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	mutex_lock(&eth->datapath_lock);
	if (eth->lan_ndev == ndev)
		eth->lan_ndev = NULL;
	mutex_unlock(&eth->datapath_lock);
}

static int
zx279133_lan_netdev_open(struct zx279133_lan_service *service,
			 struct net_device *ndev)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);
	int ret = 0;

	mutex_lock(&eth->datapath_lock);
	if (!(eth->datapath_users & ZX279133_DATAPATH_USER_LAN) ||
	    !eth->lan_datapath_ready ||
	    !READ_ONCE(eth->hardware_prepared)) {
		ret = -ENETDOWN;
		goto out_unlock;
	}

	netif_carrier_on(ndev);
	netif_tx_start_all_queues(ndev);

out_unlock:
	mutex_unlock(&eth->datapath_lock);
	return ret;
}

static int
zx279133_lan_netdev_stop(struct zx279133_lan_service *service,
			 struct net_device *ndev)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);
	bool tx_quiesced;

	mutex_lock(&eth->datapath_lock);
	if (eth->datapath_users & ZX279133_DATAPATH_USER_LAN) {
		tx_quiesced = zx279133_shared_tx_pause(eth);
		if (!tx_quiesced)
			netdev_warn(ndev, "shared TX did not quiesce at LAN stop\n");
		zx279133_shared_tx_resume(eth);
	}
	netif_tx_stop_all_queues(ndev);
	netif_carrier_off(ndev);
	mutex_unlock(&eth->datapath_lock);

	return 0;
}

static int
zx279133_lan_netdev_change_mtu(struct zx279133_lan_service *service,
			       struct net_device *ndev, int new_mtu)
{
	if (new_mtu < ETH_MIN_MTU || new_mtu > ZX279133_LAN_MAX_MTU)
		return -EINVAL;

	WRITE_ONCE(ndev->mtu, new_mtu);
	return 0;
}

static int
zx279133_lan_netdev_setup_tc(struct zx279133_lan_service *service,
			     struct net_device *ndev,
			     enum tc_setup_type type, void *type_data)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	return zx279133_flow_offload_setup_tc(eth, ndev, type, type_data);
}

static netdev_tx_t
zx279133_lan_netdev_xmit(struct zx279133_lan_service *service,
			 struct sk_buff *skb, struct net_device *ndev)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);
	netdev_tx_t ret;

	if (!(READ_ONCE(eth->datapath_users) & ZX279133_DATAPATH_USER_LAN) ||
	    !READ_ONCE(eth->lan_datapath_ready) ||
	    !READ_ONCE(eth->hardware_prepared))
		return NETDEV_TX_BUSY;

	skb->dev = eth->ndev;
	ret = zx279133_start_xmit_common(skb, eth->ndev, ndev);
	if (ret == NETDEV_TX_BUSY)
		skb->dev = ndev;

	return ret;
}

static void
zx279133_lan_netdev_tx_timeout(struct zx279133_lan_service *service,
			       struct net_device *ndev, unsigned int txqueue)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	zx279133_tx_timeout_common(eth, ndev, txqueue);
}

static void
zx279133_lan_netdev_get_stats64(struct zx279133_lan_service *service,
				struct net_device *ndev,
			       struct rtnl_link_stats64 *stats)
{
	struct zx279133_lan_netdev_priv *priv = netdev_priv(ndev);

	zx279133_fill_stats64(ndev, &priv->stats, stats);
}

static struct zx279133_eth *
zx279133_lan_service_to_eth(struct zx279133_lan_service *service)
{
	return container_of(service, struct zx279133_eth, lan_service);
}

static int zx279133_lan_datapath_get(struct zx279133_lan_service *service)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);
	bool first_user;
	int ret = 0;

	mutex_lock(&eth->datapath_lock);
	if (eth->datapath_users & ZX279133_DATAPATH_USER_LAN) {
		ret = -EBUSY;
		goto out_unlock;
	}
	first_user = !eth->datapath_users;
	if (first_user) {
		ret = zx279133_hardware_prepare(eth);
		if (ret)
			goto out_unlock;

		ret = pm_runtime_resume_and_get(eth->xpcs_mdiodev->bus->parent);
		if (ret < 0)
			goto err_hardware_unprepare;
		eth->xpcs_runtime_held = true;

		ret = zx279133_shared_idm_prepare(eth);
		if (ret)
			goto err_hardware_unprepare;
		zx279133_shared_rx_start(eth);
	}
	eth->datapath_users |= ZX279133_DATAPATH_USER_LAN;
	mutex_unlock(&eth->datapath_lock);

	return 0;

err_hardware_unprepare:
	zx279133_hardware_unprepare(eth);
	if (eth->xpcs_runtime_held) {
		pm_runtime_put(eth->xpcs_mdiodev->bus->parent);
		eth->xpcs_runtime_held = false;
	}
out_unlock:
	mutex_unlock(&eth->datapath_lock);
	return ret;
}

static void
zx279133_lan_datapath_set_ready(struct zx279133_lan_service *service,
				bool ready)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	mutex_lock(&eth->datapath_lock);
	eth->lan_datapath_ready = ready;
	if (eth->lan_ndev && netif_running(eth->lan_ndev)) {
		if (!ready) {
			netif_tx_disable(eth->lan_ndev);
			netif_carrier_off(eth->lan_ndev);
		} else if ((eth->datapath_users & ZX279133_DATAPATH_USER_LAN) &&
			   eth->hardware_prepared) {
			netif_carrier_on(eth->lan_ndev);
			netif_tx_wake_all_queues(eth->lan_ndev);
		}
	}
	mutex_unlock(&eth->datapath_lock);
}

static int
zx279133_lan_datapath_quiesce(struct zx279133_lan_service *service)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);
	bool tx_quiesced;
	int ret = 0;

	mutex_lock(&eth->datapath_lock);
	if (!(eth->datapath_users & ZX279133_DATAPATH_USER_LAN))
		goto out_unlock;
	eth->lan_datapath_ready = false;
	if (eth->lan_ndev && netif_running(eth->lan_ndev))
		netif_tx_disable(eth->lan_ndev);
	tx_quiesced = zx279133_shared_tx_pause(eth);
	if (!tx_quiesced) {
		netdev_err(eth->lan_ndev,
			   "shared TX did not quiesce before LAN teardown\n");
		ret = -ETIMEDOUT;
		goto out_unlock;
	}
	zx279133_shared_tx_resume(eth);

out_unlock:
	mutex_unlock(&eth->datapath_lock);
	return ret;
}

static void zx279133_lan_datapath_put(struct zx279133_lan_service *service)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);
	bool last_user;
	bool tx_quiesced;

	mutex_lock(&eth->datapath_lock);
	if (!(eth->datapath_users & ZX279133_DATAPATH_USER_LAN))
		goto out_unlock;
	last_user = eth->datapath_users == ZX279133_DATAPATH_USER_LAN;
	if (last_user)
		zx279133_shared_rx_stop(eth);
	tx_quiesced = zx279133_shared_tx_pause(eth);
	eth->datapath_users &= ~ZX279133_DATAPATH_USER_LAN;
	if (last_user) {
		zx279133_shared_idm_release(eth, eth->lan_ndev, tx_quiesced);
		if (eth->xpcs_runtime_held) {
			pm_runtime_put(eth->xpcs_mdiodev->bus->parent);
			eth->xpcs_runtime_held = false;
		}
	} else {
		if (!tx_quiesced)
			netdev_warn(eth->lan_ndev,
				    "shared TX did not quiesce while WAN remains active\n");
		zx279133_shared_tx_resume(eth);
	}

out_unlock:
	mutex_unlock(&eth->datapath_lock);
}

static void
zx279133_lan_set_dsa_active(struct zx279133_lan_service *service, bool active)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	WRITE_ONCE(eth->lan_dsa_active, active);
}

static u32 zx279133_lan_nppt_read(struct zx279133_lan_service *service,
				  u32 offset)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	return readl(eth->base + offset);
}

static void zx279133_lan_nppt_write(struct zx279133_lan_service *service,
				    u32 offset, u32 value)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	writel(value, eth->base + offset);
}

static void zx279133_lan_xmac_lock(struct zx279133_lan_service *service)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	mutex_lock(&eth->xmac_lock);
}

static void zx279133_lan_xmac_unlock(struct zx279133_lan_service *service)
{
	struct zx279133_eth *eth = zx279133_lan_service_to_eth(service);

	mutex_unlock(&eth->xmac_lock);
}

static int zx279133_lan_get_rxfh(struct zx279133_lan_service *service,
				 struct ethtool_rxfh_param *rxfh)
{
	return zx279133_get_rxfh_common(zx279133_lan_service_to_eth(service), 1, rxfh);
}

static int zx279133_lan_set_rxfh(struct zx279133_lan_service *service,
				 struct ethtool_rxfh_param *rxfh,
				 struct netlink_ext_ack *extack)
{
	return zx279133_set_rxfh_common(zx279133_lan_service_to_eth(service), 1,
				      rxfh, extack);
}

static int zx279133_lan_get_rxfh_fields(struct zx279133_lan_service *service,
					struct ethtool_rxfh_fields *fields)
{
	return zx279133_get_rxfh_fields_common(zx279133_lan_service_to_eth(service),
					      fields);
}

const struct zx279133_lan_service_ops zx279133_lan_service_ops = {
	.get_rxfh = zx279133_lan_get_rxfh,
	.set_rxfh = zx279133_lan_set_rxfh,
	.get_rxfh_fields = zx279133_lan_get_rxfh_fields,
	.nppt_read = zx279133_lan_nppt_read,
	.nppt_write = zx279133_lan_nppt_write,
	.xmac_lock = zx279133_lan_xmac_lock,
	.xmac_unlock = zx279133_lan_xmac_unlock,
	.datapath_get = zx279133_lan_datapath_get,
	.datapath_set_ready = zx279133_lan_datapath_set_ready,
	.datapath_quiesce = zx279133_lan_datapath_quiesce,
	.datapath_put = zx279133_lan_datapath_put,
	.set_dsa_active = zx279133_lan_set_dsa_active,
	.netdev_setup = zx279133_lan_netdev_setup,
	.netdev_teardown = zx279133_lan_netdev_teardown,
	.netdev_open = zx279133_lan_netdev_open,
	.netdev_stop = zx279133_lan_netdev_stop,
	.netdev_xmit = zx279133_lan_netdev_xmit,
	.netdev_tx_timeout = zx279133_lan_netdev_tx_timeout,
	.netdev_get_stats64 = zx279133_lan_netdev_get_stats64,
	.netdev_change_mtu = zx279133_lan_netdev_change_mtu,
	.netdev_setup_tc = zx279133_lan_netdev_setup_tc,
};
