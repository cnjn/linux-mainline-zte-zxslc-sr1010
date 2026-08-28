// SPDX-License-Identifier: GPL-2.0-only

#include <linux/dma-mapping.h>
#include <linux/bpf_trace.h>
#include <linux/etherdevice.h>
#include <linux/filter.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/prefetch.h>
#include <linux/slab.h>
#include <linux/unaligned.h>

#include <net/page_pool/helpers.h>
#include <net/xdp.h>

#include "zx279133.h"

void zx279133_idm_set_masked(struct zx279133_eth *eth, u32 mask, bool masked)
{
	void __iomem *idm = eth->base + ZX279133_IDM_BASE;
	u32 value = readl(idm + ZX279133_IDM_INT_MASK);

	if (masked)
		value |= mask;
	else
		value &= ~mask;
	writel(value, idm + ZX279133_IDM_INT_MASK);
}

static u32 zx279133_idm_rx_count(struct zx279133_eth *eth,
				 unsigned int queue)
{
	void __iomem *idm = eth->base + ZX279133_IDM_BASE;
	u32 first, second;

	first = readl(idm + 4 * ((queue >> 1) + 49));
	second = readl(idm + 4 * ((queue >> 1) + 53));
	if (queue & 1)
		return (second & GENMASK(31, 16)) | (first >> 16);
	return (second << 16) | (first & GENMASK(15, 0));
}

static u32 zx279133_idm_rx_page_key(dma_addr_t dma)
{
	return (dma - ZX279133_IDM_RX_PAYLOAD_OFFSET) >> PAGE_SHIFT;
}

static unsigned int zx279133_idm_rx_page_slot(u32 key)
{
	return hash_32(key, ZX279133_IDM_RX_PAGE_MAP_BITS);
}

static void zx279133_idm_rx_prefetch_payload(struct page *page)
{
	u8 *data = page_address(page) + ZX279133_IDM_RX_PAYLOAD_OFFSET;

	prefetch(data);
	prefetch(data + L1_CACHE_BYTES);
	prefetch(data + 2 * L1_CACHE_BYTES);
}

static int zx279133_idm_rx_track_page(struct zx279133_eth *eth,
				      struct page *page)
{
	dma_addr_t dma = page_pool_get_dma_addr(page) +
			 ZX279133_IDM_RX_PAYLOAD_OFFSET;
	u32 key;
	unsigned int slot;
	unsigned int i;

	if (upper_32_bits(dma))
		return -ERANGE;
	if (WARN_ON(eth->rx_page_map_count >= ZX279133_IDM_RX_BUFFER_COUNT))
		return -ENOSPC;

	key = zx279133_idm_rx_page_key(dma);
	slot = zx279133_idm_rx_page_slot(key);
	for (i = 0; i < ZX279133_IDM_RX_PAGE_MAP_SIZE; i++) {
		struct zx279133_rx_page_entry *entry = &eth->rx_page_map[slot];

		if (!entry->page) {
			entry->key = key;
			entry->page = page;
			eth->rx_page_map_count++;
			if (eth->rx_page_map_count > eth->rx_page_map_high_water)
				eth->rx_page_map_high_water = eth->rx_page_map_count;
			return 0;
		}
		if (WARN_ON(entry->key == key))
			return -EBUSY;
		slot = (slot + 1) & (ZX279133_IDM_RX_PAGE_MAP_SIZE - 1);
	}

	return -ENOSPC;
}

static void zx279133_idm_rx_remove_page(struct zx279133_eth *eth,
					unsigned int hole)
{
	const unsigned int mask = ZX279133_IDM_RX_PAGE_MAP_SIZE - 1;
	unsigned int scan = (hole + 1) & mask;

	while (eth->rx_page_map[scan].page) {
		u32 key = eth->rx_page_map[scan].key;
		unsigned int ideal = zx279133_idm_rx_page_slot(key);

		if (((scan - ideal) & mask) >= ((scan - hole) & mask)) {
			eth->rx_page_map[hole] = eth->rx_page_map[scan];
			hole = scan;
		}
		scan = (scan + 1) & mask;
	}
	eth->rx_page_map[hole].page = NULL;
	eth->rx_page_map[hole].key = 0;
	eth->rx_page_map_count--;
}

static int zx279133_idm_rx_post_page(struct zx279133_eth *eth,
				     struct page *page)
{
	dma_addr_t dma = page_pool_get_dma_addr(page) +
			 ZX279133_IDM_RX_PAYLOAD_OFFSET;
	int ret;

	ret = zx279133_idm_rx_track_page(eth, page);
	if (ret)
		return ret;
	WRITE_ONCE(eth->rx_normal_bp[eth->rx_bp_prod],
		   cpu_to_be32(lower_32_bits(dma)));
	eth->rx_bp_prod = (eth->rx_bp_prod + 1) &
			  (ZX279133_IDM_BP_RING_SIZE - 1);
	return 0;
}

static struct page *zx279133_idm_rx_take_page(struct zx279133_eth *eth,
					      dma_addr_t dma)
{
	u32 key;
	unsigned int slot;
	unsigned int i;
	struct page *page;

	if (dma < ZX279133_IDM_RX_PAYLOAD_OFFSET ||
	    !IS_ALIGNED(dma - ZX279133_IDM_RX_PAYLOAD_OFFSET, PAGE_SIZE))
		return NULL;

	key = zx279133_idm_rx_page_key(dma);
	slot = zx279133_idm_rx_page_slot(key);
	for (i = 0; i < ZX279133_IDM_RX_PAGE_MAP_SIZE; i++) {
		struct zx279133_rx_page_entry *entry = &eth->rx_page_map[slot];

		if (!entry->page)
			return NULL;
		if (entry->key == key) {
			page = entry->page;
			zx279133_idm_rx_remove_page(eth, slot);
			if (dma != page_pool_get_dma_addr(page) +
			    ZX279133_IDM_RX_PAYLOAD_OFFSET) {
				WARN_ON_ONCE(1);
				page_pool_recycle_direct(eth->rx_page_pool, page);
				return NULL;
			}
			return page;
		}
		slot = (slot + 1) & (ZX279133_IDM_RX_PAGE_MAP_SIZE - 1);
	}

	return NULL;
}

unsigned int zx279133_idm_tx_reclaim_locked(struct zx279133_eth *eth);

static void zx279133_idm_rx_put_vlan(struct sk_buff *skb, u16 vid)
{
	__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vid);
}

static u16 zx279133_idm_rx_restore_lan_l2(struct sk_buff *skb, u32 metadata0)
{
	u16 l3_prefix = metadata0 >> 16;
	u16 protocol;

	if ((l3_prefix & 0xf000) == 0x4000) {
		protocol = ETH_P_IP;
	} else if ((l3_prefix & 0xf000) == 0x6000) {
		protocol = ETH_P_IPV6;
	} else if (l3_prefix == ARPHRD_ETHER &&
		   get_unaligned_be16(skb->data + 2 * ETH_ALEN) == ETH_P_IP &&
		   skb->data[2 * ETH_ALEN + 2] == ETH_ALEN &&
		   skb->data[2 * ETH_ALEN + 3] == 4) {
		protocol = ETH_P_ARP;
	} else {
		return 0;
	}

	skb_put(skb, VLAN_HLEN);
	memmove(skb->data + 2 * ETH_ALEN + VLAN_HLEN,
		skb->data + 2 * ETH_ALEN,
		skb->len - 2 * ETH_ALEN - VLAN_HLEN);
	put_unaligned_be16(protocol, skb->data + 2 * ETH_ALEN);
	put_unaligned_be16(l3_prefix,
			   skb->data + 2 * ETH_ALEN + sizeof(protocol));

	return VLAN_HLEN;
}

static bool zx279133_idm_rx_normalize_vlan(struct vlan_ethhdr *vhdr)
{
	u16 tci;

	if (vhdr->h_vlan_proto != htons(ETH_P_8021Q))
		return false;

	tci = ntohs(vhdr->h_vlan_TCI);
	if ((tci & VLAN_VID_MASK) == ZX279133_LAN_INGRESS_VID) {
		tci = (tci & ~VLAN_VID_MASK) | ZX279133_LAN_VID;
		vhdr->h_vlan_TCI = htons(tci);
	}

	return true;
}

static void zx279133_idm_rx_sync_for_device(struct zx279133_eth *eth,
					    struct page *page, u16 len)
{
	dma_addr_t dma = page_pool_get_dma_addr(page);

	dma_sync_single_range_for_device(eth->dev, dma,
					 ZX279133_IDM_RX_PAYLOAD_OFFSET, len,
					 DMA_BIDIRECTIONAL);
}

static int zx279133_idm_rx_process_queue(struct zx279133_eth *eth,
					 struct napi_struct *napi,
					 unsigned int queue, u16 count,
					 bool *xdp_tx, bool *xdp_redirect)
{
	struct net_device *ndev = eth->ndev;
	void __iomem *idm = eth->base + ZX279133_IDM_BASE;
	u16 refill = 0;
	u16 rx_work = 0;

	while (rx_work < count) {
		struct zx279133_idm_desc *desc;
		struct page *replacement = NULL;
		struct page *page;
		struct sk_buff *skb = NULL;
		void *data;
		dma_addr_t dma;
		u32 word1;
		u16 desc_index = eth->rx_cons[queue];
		u16 refill_before = refill;
		u16 len;
		bool lan_source;
		bool valid_dma;

		desc = eth->rx_descs + queue * ZX279133_IDM_RX_RING_SIZE +
			desc_index;
		dma = le32_to_cpu(READ_ONCE(desc->address));
		if (!dma) {
			u64_stats_inc(&eth->rx_desc_not_ready);
			break;
		}
		eth->rx_cons[queue] = (eth->rx_cons[queue] + 1) &
					      (ZX279133_IDM_RX_RING_SIZE - 1);
		dma_rmb();
		valid_dma = dma >= ZX279133_IDM_RX_PAYLOAD_OFFSET &&
			    IS_ALIGNED(dma - ZX279133_IDM_RX_PAYLOAD_OFFSET,
				       PAGE_SIZE);
		if (likely(valid_dma)) {
			u32 key = zx279133_idm_rx_page_key(dma);

			prefetch(&eth->rx_page_map[zx279133_idm_rx_page_slot(key)]);
		}
		word1 = le32_to_cpu(READ_ONCE(desc->length_flags));
		len = word1 & GENMASK(13, 0);
		if (unlikely(!valid_dma)) {
			u64_stats_inc(&eth->rx_invalid_dma);
			page = NULL;
		} else {
			page = zx279133_idm_rx_take_page(eth, dma);
			if (unlikely(!page))
				u64_stats_inc(&eth->rx_page_lookup_misses);
		}

		if (!page) {
			zx279133_stats_rx_error(eth, ndev);
			if (net_ratelimit())
				netdev_warn(ndev,
					    "RX queue %u descriptor %u has unknown DMA %pad (word1 %#x, tracked %u)\n",
					    queue, desc_index, &dma, word1,
					    eth->rx_page_map_count);
			goto release_desc;
		}

		if (word1 & BIT(14)) {
			u64_stats_inc(&eth->rx_jumbo_drops);
			zx279133_stats_rx_dropped(eth, ndev);
			zx279133_stats_rx_error(eth, ndev);
			goto reuse_page;
		}
		if (word1 & BIT(15)) {
			u64_stats_inc(&eth->rx_descriptor_flag_drops);
			zx279133_stats_rx_dropped(eth, ndev);
			goto reuse_page;
		}
		if (len < ETH_HLEN || len > ZX279133_IDM_RX_FRAME_LIMIT) {
			zx279133_stats_rx_error(eth, ndev);
			zx279133_stats_rx_length_error(eth, ndev);
			goto reuse_page;
		}
		lan_source = queue < ZX279133_LAN_RX_QUEUE_COUNT;

		replacement = page_pool_dev_alloc_pages(eth->rx_page_pool);
		if (!replacement) {
			u64_stats_inc(&eth->rx_page_alloc_failures);
		} else if (zx279133_idm_rx_post_page(eth, replacement)) {
			u64_stats_inc(&eth->rx_refill_post_failures);
			page_pool_recycle_direct(eth->rx_page_pool, replacement);
			replacement = NULL;
		}
		if (replacement) {
			refill++;
		}
		page_pool_dma_sync_for_cpu(eth->rx_page_pool, page, 0, len);
		zx279133_idm_rx_prefetch_payload(page);
		data = page_address(page) + ZX279133_IDM_RX_PAYLOAD_OFFSET;

		if (!lan_source) {
			struct bpf_prog *xdp_prog =
				rcu_dereference_bh(eth->xdp_prog);

			if (xdp_prog) {
				struct xdp_buff xdp;
				u32 act;

				xdp_init_buff(&xdp, ZX279133_RX_PAGE_SIZE,
					      &eth->xdp_rxq);
				xdp_prepare_buff(&xdp, page_address(page),
						 ZX279133_IDM_RX_PAYLOAD_OFFSET,
						 len, false);
				act = bpf_prog_run_xdp(xdp_prog, &xdp);
				switch (act) {
				case XDP_PASS:
					u64_stats_inc(&eth->xdp_pass);
					data = xdp.data;
					len = xdp.data_end - xdp.data;
					break;
				case XDP_TX: {
					struct xdp_frame *xdpf;

					xdpf = xdp_convert_buff_to_frame(&xdp);
					if (xdpf && !zx279133_xdp_enqueue(eth, xdpf)) {
						u64_stats_inc(&eth->xdp_tx);
						*xdp_tx = true;
						goto release_desc;
					}
					u64_stats_inc(&eth->xdp_aborted);
					trace_xdp_exception(ndev, xdp_prog, act);
					if (xdpf) {
						xdp_return_frame_rx_napi(xdpf);
						goto release_desc;
					}
					goto xdp_recycle;
				}
				case XDP_REDIRECT:
					if (!xdp_do_redirect(ndev, &xdp, xdp_prog)) {
						u64_stats_inc(&eth->xdp_redirect);
						*xdp_redirect = true;
						goto release_desc;
					}
					u64_stats_inc(&eth->xdp_aborted);
					trace_xdp_exception(ndev, xdp_prog, act);
					goto xdp_recycle;
				case XDP_ABORTED:
					u64_stats_inc(&eth->xdp_aborted);
					trace_xdp_exception(ndev, xdp_prog, act);
					goto xdp_recycle;
				case XDP_DROP:
					u64_stats_inc(&eth->xdp_drop);
					goto xdp_recycle;
				default:
					u64_stats_inc(&eth->xdp_aborted);
					bpf_warn_invalid_xdp_action(ndev, xdp_prog,
								    act);
					trace_xdp_exception(ndev, xdp_prog, act);
					goto xdp_recycle;
				}
			}
		}

		if (replacement) {
			skb = napi_build_skb(page_address(page),
					     ZX279133_RX_PAGE_SIZE);
			if (!skb) {
				u64_stats_inc(&eth->rx_skb_alloc_failures);
				zx279133_stats_rx_dropped(eth, ndev);
				page_pool_recycle_direct(eth->rx_page_pool, page);
				goto release_desc;
			}
			skb_mark_for_recycle(skb);
			skb_reserve(skb, data - page_address(page));
			skb_put(skb, len);
		} else {
			u64_stats_inc(&eth->rx_copy_fallbacks);
			skb = napi_alloc_skb(napi, len + VLAN_HLEN + NET_IP_ALIGN);
			if (skb) {
				skb_reserve(skb, NET_IP_ALIGN);
				memcpy(skb_put(skb, len), data, len);
			} else {
				u64_stats_inc(&eth->rx_skb_alloc_failures);
				zx279133_stats_rx_dropped(eth, ndev);
			}
			zx279133_idm_rx_sync_for_device(eth, page, len);
			if (zx279133_idm_rx_post_page(eth, page)) {
				u64_stats_inc(&eth->rx_refill_post_failures);
				page_pool_recycle_direct(eth->rx_page_pool, page);
				zx279133_stats_rx_error(eth, ndev);
			} else {
				refill++;
			}
		}

		if (skb) {
			struct net_device *rx_ndev = ndev;
			bool lan_vlan_active;
			bool lan_dsa_active;

			lan_dsa_active = READ_ONCE(eth->lan_dsa_active);
			lan_vlan_active = READ_ONCE(eth->lan_vlan62_active);
			/* Factory CPU RX queues 0..7 belong to the external switch;
			 * queues 8..15 belong to the other CPU datapath group. Unlike the
			 * parsed descriptor fields, that split remains stable when CPU8
			 * strips the LAN transport VLAN before PPU.
			 */
			if (lan_source)
				len += zx279133_idm_rx_restore_lan_l2(
					skb, le32_to_cpu(READ_ONCE(desc->metadata[0])));
			if (lan_source && !lan_dsa_active && !lan_vlan_active) {
				if (eth->lan_ndev)
					zx279133_stats_rx_dropped(eth, eth->lan_ndev);
				dev_kfree_skb_any(skb);
				goto release_desc;
			}

			if (lan_dsa_active && lan_source && eth->lan_ndev) {
				bool vlan_header = false;
				u16 transport_vid = ZX279133_LAN_VID;

				rx_ndev = eth->lan_ndev;
				if (len >= sizeof(struct vlan_ethhdr)) {
					struct vlan_ethhdr *vhdr =
						(struct vlan_ethhdr *)skb->data;
					u16 tci = ntohs(vhdr->h_vlan_TCI);
					u16 ingress_vid = tci & VLAN_VID_MASK;

					if (eth_type_vlan(vhdr->h_vlan_proto) &&
					    (ingress_vid == ZX279133_LAN_INGRESS_VID ||
					     (ingress_vid >= ZX279133_LAN_TRANSPORT_VID_MIN &&
					      ingress_vid <= ZX279133_LAN_TRANSPORT_VID_MAX))) {
						/* The switch's private transport VID may arrive
						 * as VID 1 or another transport VID. Normalize
						 * VID 1 to the active fast-path port; preserve a
						 * tagged transport VID for the other DSA ports.
						 */
						vlan_header = true;
						if (ingress_vid >=
						    ZX279133_LAN_TRANSPORT_VID_MIN)
							transport_vid = ingress_vid;
						tci = (tci & ~VLAN_VID_MASK) |
						      transport_vid;
						vhdr->h_vlan_TCI = htons(tci);
					}
				}
				/* Preserve a customer VLAN already present in-band by
				 * stacking the private transport VID outside it. The DSA
				 * tagger removes only this outer sideband tag.
				 */
				if (!vlan_header)
					zx279133_idm_rx_put_vlan(skb, transport_vid);
			} else if (lan_vlan_active && lan_source) {
				bool vlan_header = false;

				if (len >= sizeof(struct vlan_ethhdr)) {
					struct vlan_ethhdr *vhdr;

					vhdr = (struct vlan_ethhdr *)skb->data;
					vlan_header = zx279133_idm_rx_normalize_vlan(vhdr);
				}
				if (!vlan_header)
					zx279133_idm_rx_put_vlan(skb, ZX279133_LAN_VID);
			}
			skb->protocol = eth_type_trans(skb, rx_ndev);
			napi_gro_receive(napi, skb);
			dev_sw_netstats_rx_add(rx_ndev, len);
		}
		goto release_desc;

xdp_recycle:
		if (replacement) {
			page_pool_recycle_direct(eth->rx_page_pool, page);
			goto release_desc;
		}
		zx279133_idm_rx_sync_for_device(eth, page, len);

reuse_page:
		if (zx279133_idm_rx_post_page(eth, page)) {
			u64_stats_inc(&eth->rx_refill_post_failures);
			page_pool_recycle_direct(eth->rx_page_pool, page);
			zx279133_stats_rx_error(eth, ndev);
		} else {
			refill++;
		}

release_desc:
		if (refill == refill_before) {
			u64_stats_inc(&eth->rx_refill_shortfalls);
			if (WARN_ON_ONCE(eth->rx_refill_deficit >=
					 ZX279133_IDM_RX_BUFFER_COUNT)) {
				eth->rx_refill_deficit =
					ZX279133_IDM_RX_BUFFER_COUNT;
			} else {
				eth->rx_refill_deficit++;
				if (eth->rx_refill_deficit >
				    eth->rx_refill_deficit_high_water)
					eth->rx_refill_deficit_high_water =
						eth->rx_refill_deficit;
			}
		}
		WRITE_ONCE(desc->address, 0);
		rx_work++;
	}

	if (rx_work) {
		dma_wmb();
		writel(rx_work | (queue << 12), idm + ZX279133_IDM_RX_RELEASE);
		u64_stats_add(&eth->rx_release_published, rx_work);
		if (refill) {
			writel(refill, idm + ZX279133_IDM_BP_REFILL);
			u64_stats_add(&eth->rx_refill_published, refill);
		}
	}

	return rx_work;
}

static unsigned int zx279133_idm_rx_recover_refill(struct zx279133_eth *eth)
{
	void __iomem *idm = eth->base + ZX279133_IDM_BASE;
	unsigned int recovered = 0;

	if (!eth->rx_refill_deficit)
		return 0;

	u64_stats_inc(&eth->rx_refill_recovery_attempts);
	while (eth->rx_refill_deficit &&
	       recovered < ZX279133_RX_REFILL_RECOVERY_BATCH) {
		struct page *page;

		/* An unknown descriptor DMA can leave its original page tracked.
		 * Without evidence identifying that stale entry, fail closed rather
		 * than overfilling hardware ownership or repeatedly warning.
		 */
		if (eth->rx_page_map_count >= ZX279133_IDM_RX_BUFFER_COUNT) {
			u64_stats_inc(&eth->rx_refill_recovery_failures);
			break;
		}
		page = page_pool_dev_alloc_pages(eth->rx_page_pool);
		if (!page) {
			u64_stats_inc(&eth->rx_page_alloc_failures);
			u64_stats_inc(&eth->rx_refill_recovery_failures);
			break;
		}
		if (zx279133_idm_rx_post_page(eth, page)) {
			u64_stats_inc(&eth->rx_refill_post_failures);
			u64_stats_inc(&eth->rx_refill_recovery_failures);
			page_pool_recycle_direct(eth->rx_page_pool, page);
			break;
		}
		eth->rx_refill_deficit--;
		recovered++;
	}

	if (recovered) {
		dma_wmb();
		writel(recovered, idm + ZX279133_IDM_BP_REFILL);
		u64_stats_add(&eth->rx_refill_published, recovered);
		u64_stats_add(&eth->rx_refill_recovery_pages, recovered);
	}

	return recovered;
}

void zx279133_idm_rx_refill_work(struct work_struct *work)
{
	struct zx279133_eth *eth =
		container_of(to_delayed_work(work), struct zx279133_eth,
			     rx_refill_work);

	if (!READ_ONCE(eth->rx_running) || !READ_ONCE(eth->napi_enabled) ||
	    !READ_ONCE(eth->rx_refill_deficit))
		return;

	atomic64_inc(&eth->rx_refill_retry_work_runs);
	napi_schedule(&eth->napi);
}

static const u8 zx279133_idm_rx_poll_order[] = {
	7, 15, 6, 14, 5, 13, 4, 12, 3, 11, 2, 10, 1, 9, 0, 8,
};

int zx279133_idm_rx_poll(struct napi_struct *napi, int budget)
{
	struct zx279133_eth *eth = container_of(napi, struct zx279133_eth,
					       napi);
	bool xdp_redirect = false;
	bool xdp_tx = false;
	u8 cursor = eth->rx_poll_cursor;
	int work = 0;
	int scanned;

	/* A zero budget is TX-completion-only; page_pool APIs are forbidden. */
	if (unlikely(!budget)) {
		spin_lock_bh(&eth->tx_lock);
		zx279133_idm_tx_reclaim_locked(eth);
		spin_unlock_bh(&eth->tx_lock);
		return 0;
	}

	u64_stats_update_begin(&eth->rx_stats_sync);
	u64_stats_inc(&eth->rx_napi_polls);
	for (scanned = 0;
	     scanned < ARRAY_SIZE(zx279133_idm_rx_poll_order) && work < budget;
	     scanned++) {
		u8 queue = zx279133_idm_rx_poll_order[cursor];
		u32 packed = zx279133_idm_rx_count(eth, queue & 7);
		u16 count;

		if (queue < 8)
			count = packed & 0xffff;
		else
			count = packed >> 16;
		count = min_t(u16, count, budget - work);
		if (count)
			work += zx279133_idm_rx_process_queue(eth, napi, queue,
							      count, &xdp_tx,
							      &xdp_redirect);
		cursor = (cursor + 1) &
			 (ARRAY_SIZE(zx279133_idm_rx_poll_order) - 1);
	}
	eth->rx_poll_cursor = cursor;
	if (xdp_tx)
		zx279133_xdp_flush(eth);
	if (xdp_redirect)
		xdp_do_flush();

	spin_lock_bh(&eth->tx_lock);
	zx279133_idm_tx_reclaim_locked(eth);
	spin_unlock_bh(&eth->tx_lock);

	zx279133_idm_rx_recover_refill(eth);
	if (eth->rx_refill_deficit &&
	    eth->rx_page_map_count < ZX279133_IDM_RX_BUFFER_COUNT &&
	    READ_ONCE(eth->rx_running))
		mod_delayed_work(system_wq, &eth->rx_refill_work,
				 msecs_to_jiffies(ZX279133_RX_REFILL_RETRY_MS));

	u64_stats_add(&eth->rx_napi_work, work);
	if (work >= budget)
		u64_stats_inc(&eth->rx_napi_budget_exhaustions);

	if (work < budget && napi_complete_done(napi, work)) {
		unsigned long flags;

		spin_lock_irqsave(&eth->irq_lock, flags);
		if (eth->rx_running)
			zx279133_idm_set_masked(eth, ZX279133_IDM_NAPI_MASK,
						false);
		spin_unlock_irqrestore(&eth->irq_lock, flags);
	}

	u64_stats_update_end(&eth->rx_stats_sync);
	return work;
}

irqreturn_t zx279133_idm_rx_irq(int irq, void *data)
{
	struct zx279133_eth *eth = data;
	unsigned long flags;
	bool running;

	atomic64_inc(&eth->rx_irq_count);
	spin_lock_irqsave(&eth->irq_lock, flags);
	running = eth->rx_running;
	zx279133_idm_set_masked(eth, ZX279133_IDM_DIRECT_RX_MASK, true);
	spin_unlock_irqrestore(&eth->irq_lock, flags);

	if (running)
		napi_schedule_irqoff(&eth->napi);

	return IRQ_HANDLED;
}

irqreturn_t zx279133_idm_local_irq(int irq, void *data)
{
	struct zx279133_eth *eth = data;
	unsigned long flags;
	bool running;

	atomic64_inc(&eth->idm_local_irq_count);
	spin_lock_irqsave(&eth->irq_lock, flags);
	running = eth->rx_running;
	zx279133_idm_set_masked(eth, ZX279133_IDM_LOCAL_MASK, true);
	spin_unlock_irqrestore(&eth->irq_lock, flags);

	if (running)
		napi_schedule_irqoff(&eth->napi);

	return IRQ_HANDLED;
}

static void zx279133_idm_rx_put_active_pages(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ZX279133_IDM_RX_PAGE_MAP_SIZE; i++) {
		struct zx279133_rx_page_entry *entry = &eth->rx_page_map[i];

		if (!entry->page)
			continue;
		page_pool_put_full_page(eth->rx_page_pool, entry->page, false);
		entry->page = NULL;
		entry->key = 0;
	}
	eth->rx_page_map_count = 0;
}

int zx279133_idm_rx_prepare(struct zx279133_eth *eth)
{
	void __iomem *idm = eth->base + ZX279133_IDM_BASE;
	size_t desc_size = sizeof(struct zx279133_idm_desc) *
			   ZX279133_IDM_RX_QUEUES * ZX279133_IDM_RX_RING_SIZE;
	unsigned int i;
	int ret;

	if (WARN_ON(eth->rx_page_map_count))
		zx279133_idm_rx_put_active_pages(eth);
	memset(eth->rx_descs, 0, desc_size);
	memset(eth->rx_normal_bp, 0, ZX279133_IDM_FREE_RING_SIZE);
	memset(eth->rx_cons, 0, sizeof(eth->rx_cons));
	eth->rx_bp_prod = 0;
	eth->rx_poll_cursor = 0;
	eth->rx_refill_deficit = 0;

	for (i = 0; i < ZX279133_IDM_RX_BUFFER_COUNT; i++) {
		struct page *page = page_pool_dev_alloc_pages(eth->rx_page_pool);

		if (!page) {
			ret = -ENOMEM;
			goto err_put_pages;
		}
		ret = zx279133_idm_rx_post_page(eth, page);
		if (ret) {
			page_pool_put_full_page(eth->rx_page_pool, page, false);
			goto err_put_pages;
		}
	}

	dma_wmb();
	writel(ZX279133_IDM_RX_BUFFER_COUNT, idm + ZX279133_IDM_BP_REFILL);
	eth->rx_prepared = true;
	return 0;

err_put_pages:
	zx279133_idm_rx_put_active_pages(eth);
	return ret;
}

void zx279133_idm_rx_release(struct zx279133_eth *eth)
{
	cancel_delayed_work_sync(&eth->rx_refill_work);
	zx279133_idm_rx_put_active_pages(eth);
	eth->rx_prepared = false;
}

void zx279133_idm_tx_flush_locked(struct zx279133_eth *eth)
{
	u32 count = eth->tx_notify_pending;

	if (!count)
		return;
	/* Vendor idm_cpu_nb_tx_update() requires DSB ST before publication. */
	wmb();
	writel(count << ZX279133_IDM_TX_DOORBELL_COUNT_SHIFT,
	       eth->base + ZX279133_IDM_BASE +
	       zx279133_idm_tx_doorbell_reg(zx279133_tx_queue));
	eth->tx_notify_pending = 0;
	eth->tx_doorbell_writes++;
	eth->tx_doorbell_descs += count;
}

unsigned int zx279133_idm_tx_reclaim_locked(struct zx279133_eth *eth)
{
	struct netdev_queue *txq = netdev_get_tx_queue(eth->ndev, 0);
	u16 done = readl(eth->base + ZX279133_IDM_BASE +
			 zx279133_idm_tx_done_reg(zx279133_tx_queue)) & 0xffff;
	u16 completed = done - eth->tx_done;
	u16 reclaimed;
	u32 bytes = 0;
	u16 packets = 0;

	eth->tx_reclaim_polls++;
	if (completed > eth->tx_pending) {
		if (net_ratelimit())
			netdev_warn(eth->ndev,
				    "invalid TX completion delta %u (pending %u)\n",
				    completed, eth->tx_pending);
		completed = eth->tx_pending;
	}
	eth->tx_done = done;
	reclaimed = completed;

	while (completed--) {
		struct zx279133_tx_slot *slot =
			&eth->tx_slots[eth->tx_consumer];

		if (slot->skb) {
			struct net_device *ndev = slot->ndev ?: eth->ndev;

			if (slot->dma_mapped)
				dma_unmap_single(eth->dev, slot->dma, slot->len,
						 DMA_TO_DEVICE);
			dev_sw_netstats_tx_add(ndev, 1, slot->len);
			bytes += slot->len;
			packets++;
			dev_consume_skb_any(slot->skb);
		} else if (slot->xdpf) {
			if (slot->dma_mapped)
				dma_unmap_single(eth->dev, slot->dma, slot->len,
						 DMA_TO_DEVICE);
			dev_sw_netstats_tx_add(eth->ndev, 1, slot->len);
			xdp_return_frame(slot->xdpf);
		}
		memset(slot, 0, sizeof(*slot));
		eth->tx_consumer = (eth->tx_consumer + 1) &
					   (ZX279133_IDM_TX_DEPTH - 1);
		eth->tx_pending--;
	}

	/* BQL requires one completion report per reclaim round, not per skb. */
	if (packets)
		netdev_tx_completed_queue(txq, packets, bytes);

	if (!READ_ONCE(eth->tx_stopping) &&
	    eth->tx_pending < ZX279133_IDM_TX_DEPTH - 1) {
		if (netif_queue_stopped(eth->ndev))
			netif_wake_queue(eth->ndev);
		if ((READ_ONCE(eth->datapath_users) &
		     ZX279133_DATAPATH_USER_LAN) &&
		    READ_ONCE(eth->lan_datapath_ready) && eth->lan_ndev &&
		    netif_running(eth->lan_ndev) &&
		    netif_queue_stopped(eth->lan_ndev))
			netif_wake_queue(eth->lan_ndev);
	}

	return reclaimed;
}

void zx279133_idm_tx_reclaim_work(struct work_struct *work)
{
	struct zx279133_eth *eth =
		container_of(to_delayed_work(work), struct zx279133_eth,
			     tx_reclaim_work);
	unsigned int reclaimed = 0;
	bool rearm = false;

	spin_lock_bh(&eth->tx_lock);
	if (!READ_ONCE(eth->tx_stopping) && eth->tx_prepared &&
	    READ_ONCE(eth->hardware_prepared)) {
		eth->tx_reclaim_work_runs++;
		zx279133_idm_tx_flush_locked(eth);
		reclaimed = zx279133_idm_tx_reclaim_locked(eth);
		eth->tx_reclaim_work_packets += reclaimed;
		rearm = eth->tx_pending;
	}
	spin_unlock_bh(&eth->tx_lock);

	if (rearm && !READ_ONCE(eth->tx_stopping))
		mod_delayed_work(system_wq, &eth->tx_reclaim_work,
				 msecs_to_jiffies(ZX279133_TX_RECLAIM_DELAY_MS));
}

bool zx279133_idm_tx_drain(struct zx279133_eth *eth)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(100);

	do {
		spin_lock_bh(&eth->tx_lock);
		zx279133_idm_tx_flush_locked(eth);
		zx279133_idm_tx_reclaim_locked(eth);
		if (!eth->tx_pending) {
			spin_unlock_bh(&eth->tx_lock);
			return true;
		}
		spin_unlock_bh(&eth->tx_lock);
		usleep_range(100, 200);
	} while (time_before(jiffies, timeout));

	return false;
}

static const u32 zx279133_idm_cfg_offsets[] = {
	0x0000, 0x0008, 0x000c, 0x0010,
	0x0014, 0x0018, 0x001c, 0x0020, 0x0024,
	0x0028, 0x002c, 0x0030, 0x0034, 0x0038,
	0x0054, 0x0058, 0x005c, 0x0060,
	0x0070, 0x0074,
	0x0090, 0x0094,
	0x00c0, 0x0124,
	0x03fc, 0x05c0,
	/* Buffer pools, free rings, and interrupt-group configuration. */
	0x0040, 0x0044, 0x0048, 0x004c, 0x0050,
	0x006c,
	0x0104, 0x0108, 0x0118, 0x011c, 0x0408,
	0x010c, 0x0110, 0x040c,
};

/*
 * Exact CPU133 vendor-Linux idm_init() register image, recovered from
 * plat_132.ko disassembly and kernel-2b5.elf data:
 *  - control word RMW: |= 0xf0000, (v & 0xf00fffff) | 0xf00000, |= 0x3000,
 *    then = (v & 0x8fffffff) | (uIDM_RX_CFG_DEPTH << 28)
 *  - fixed: 0x054..0x060 watermarks, 0x014..0x034 = 0x800080,
 *    0x038 = 50000, 0x010 = 128, 0x090 = 20, 0x094 = 1,
 *    0x3fc = 0x0f49, 0x074 = 0x210, 0x070 = RX depth - 1,
 *    0x0c0 = descriptor mode, 0x00c = TX config depth << 16, 0x124 = 0,
 *    and the CPU133-only 0x5c0 = 7.
 */

static void zx279133_idm_tx_configure(struct zx279133_eth *eth)
{
	void __iomem *idm = eth->base + ZX279133_IDM_BASE;
	u32 value;
	int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_idm_cfg_offsets); i++)
		eth->idm_cfg_saved[i] = readl(idm + zx279133_idm_cfg_offsets[i]);

	value = eth->idm_cfg_saved[0];
	value |= 0x000f0000;
	value = (value & 0xf00fffff) | 0x00f00000;
	value |= 0x00003000;
	value = (value & 0x8fffffff) | (ZX279133_IDM_RX_CFG_DEPTH << 28);
	if (zx279133_idm_cfg_bit24)
		value |= BIT(24);
	writel(value, idm + 0x0000);

	writel(lower_32_bits(eth->rx_descs_dma), idm + 0x0008);
	writel(ZX279133_IDM_TX_CFG_DEPTH << 16, idm + 0x000c);
	writel(128, idm + 0x0010);
	for (i = 0x14; i <= 0x34; i += 4)
		writel(0x00800080, idm + i);
	writel(50000, idm + 0x0038);
	writel(0x06060606, idm + 0x0054);
	writel(0x00060606, idm + 0x0058);
	writel(0x07070707, idm + 0x005c);
	writel(0x07070707, idm + 0x0060);
	writel(ZX279133_IDM_RX_QUEUE_DESC_DEPTH - 1, idm + 0x0070);
	writel(0x00000210, idm + 0x0074);
	writel(20, idm + 0x0090);
	writel(1, idm + 0x0094);
	writel(ZX279133_IDM_DESC_MODE, idm + 0x00c0);
	writel(0, idm + 0x0124);
	writel(0x00000f49, idm + 0x03fc);
	writel(7, idm + 0x05c0);

	/*
	 * Remaining vendor idm_init()/idm_cfg_int() register image: the
	 * interrupt mask/info words, the normal buffer length, the buffer
	 * pool configuration, and the normal/jumbo/extra free-ring bases. The
	 * active normal ring is DMA coherent; unused rings retain vendor offsets.
	 */
	writel(0x07ffffff, idm + 0x0040);
	writel(ZX279133_IDM_DIRECT_RX_MASK, idm + 0x0044);
	writel(0x00ff0000, idm + 0x0048);
	/* No vendor buffer-release callback is used by the page_pool path. */
	writel(0, idm + 0x004c);
	writel(ZX279133_IDM_LOCAL_MASK, idm + 0x0050);
	writel(0x000007c1, idm + 0x006c);

	writel(lower_32_bits(eth->rx_normal_bp_dma), idm + 0x0104);
	writel(lower_32_bits(eth->idm_base + ZX279133_IDM_FREE_RING1),
	       idm + 0x0108);
	writel(lower_32_bits(eth->idm_base + ZX279133_IDM_FREE_RING2),
	       idm + 0x0118);
	writel(lower_32_bits(eth->idm_base + ZX279133_IDM_FREE_RING3),
	       idm + 0x011c);
	writel(lower_32_bits(eth->idm_base + ZX279133_IDM_FREE_RING4),
	       idm + 0x0408);
	writel(0x00003d00, idm + 0x010c);
	writel(0x00003d00, idm + 0x0110);
	writel(0x00003d00, idm + 0x040c);
}

static void zx279133_idm_tx_deconfigure(struct zx279133_eth *eth)
{
	void __iomem *idm = eth->base + ZX279133_IDM_BASE;
	int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_idm_cfg_offsets); i++)
		writel(eth->idm_cfg_saved[i], idm + zx279133_idm_cfg_offsets[i]);
}

int zx279133_idm_tx_prepare(struct zx279133_eth *eth)
{
	size_t size = sizeof(*eth->tx_descs) * ZX279133_IDM_TX_QUEUES *
		      ZX279133_IDM_TX_DEPTH;
	u32 ready;

	if (zx279133_tx_queue >= ZX279133_IDM_TX_QUEUES)
		return dev_err_probe(eth->dev, -EINVAL,
				     "invalid IDM TX queue %u\n",
				     zx279133_tx_queue);

	ready = readl(eth->base + ZX279133_NP_READY);
	if ((ready & ZX279133_NP_READY_MASK) != ZX279133_NP_READY_MASK)
		return dev_err_probe(eth->dev, -EIO,
				     "NPPT handoff is not ready: %#x\n", ready);

	memset(eth->tx_descs, 0, size);

	eth->idm_tx_base_saved = readl(eth->base + ZX279133_IDM_BASE +
					ZX279133_IDM_TX_BASE);
	eth->tx_done = readl(eth->base + ZX279133_IDM_BASE +
			     zx279133_idm_tx_done_reg(zx279133_tx_queue)) & 0xffff;
	eth->tx_producer = eth->tx_done & (ZX279133_IDM_TX_DEPTH - 1);
	eth->tx_consumer = eth->tx_producer;
	eth->tx_pending = 0;
	eth->tx_notify_pending = 0;
	memset(eth->tx_slots, 0,
	       sizeof(*eth->tx_slots) * ZX279133_IDM_TX_DEPTH);
	netdev_tx_reset_queue(netdev_get_tx_queue(eth->ndev, 0));
	if (eth->lan_ndev)
		netdev_tx_reset_queue(netdev_get_tx_queue(eth->lan_ndev, 0));
	dev_dbg(eth->dev,
		"IDM TX: queue %u, port 0x%02x, selector 0x%02x, pon_control %#x, word4 bit23 %u, cfg bit24 %u (doorbell %#x, done %#x)\n",
		zx279133_tx_queue, zx279133_tx_port & 0xff,
		zx279133_tx_selector & 0xff, zx279133_tx_pon_control,
		zx279133_tx_word4_bit23 ? 1 : 0,
		zx279133_idm_cfg_bit24 ? 1 : 0,
		zx279133_idm_tx_doorbell_reg(zx279133_tx_queue),
		zx279133_idm_tx_done_reg(zx279133_tx_queue));
	zx279133_idm_tx_configure(eth);
	writel(lower_32_bits(eth->tx_descs_dma),
	       eth->base + ZX279133_IDM_BASE + ZX279133_IDM_TX_BASE);
	eth->tx_prepared = true;

	return 0;
}

void zx279133_idm_tx_deactivate(struct zx279133_eth *eth)
{
	writel(eth->idm_tx_base_saved,
	       eth->base + ZX279133_IDM_BASE + ZX279133_IDM_TX_BASE);
	zx279133_idm_tx_deconfigure(eth);
}

void zx279133_idm_tx_release(struct zx279133_eth *eth, bool hardware_alive)
{
	struct netdev_queue *txq = netdev_get_tx_queue(eth->ndev, 0);
	u32 completed_bytes = 0;
	u16 completed_packets = 0;

	if (!eth->tx_prepared)
		return;

	spin_lock_bh(&eth->tx_lock);
	if (hardware_alive) {
		zx279133_idm_tx_flush_locked(eth);
		zx279133_idm_tx_reclaim_locked(eth);
	} else {
		eth->tx_notify_pending = 0;
	}
	while (eth->tx_pending) {
		struct zx279133_tx_slot *slot =
			&eth->tx_slots[eth->tx_consumer];
		struct zx279133_tx_slot owner = *slot;

		memset(slot, 0, sizeof(*slot));
		eth->tx_consumer = (eth->tx_consumer + 1) &
					   (ZX279133_IDM_TX_DEPTH - 1);
		eth->tx_pending--;
		spin_unlock_bh(&eth->tx_lock);

		if (owner.skb) {
			struct net_device *ndev = owner.ndev ?: eth->ndev;

			if (owner.dma_mapped)
				dma_unmap_single(eth->dev, owner.dma, owner.len,
						 DMA_TO_DEVICE);
			zx279133_stats_tx_dropped(eth, ndev);
			completed_packets++;
			completed_bytes += owner.len;
			dev_kfree_skb_any(owner.skb);
		} else if (owner.xdpf) {
			if (owner.dma_mapped)
				dma_unmap_single(eth->dev, owner.dma, owner.len,
						 DMA_TO_DEVICE);
			xdp_return_frame(owner.xdpf);
		}
		spin_lock_bh(&eth->tx_lock);
	}
	spin_unlock_bh(&eth->tx_lock);
	if (completed_packets)
		netdev_tx_completed_queue(txq, completed_packets,
					  completed_bytes);

	if (hardware_alive)
		zx279133_idm_tx_deactivate(eth);
	eth->tx_prepared = false;
}
