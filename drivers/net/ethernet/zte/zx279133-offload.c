// SPDX-License-Identifier: GPL-2.0-only
/* ZTE ZX279133 NPPT IPv4/IPv6 flow-table offload. */

#include <linux/bitmap.h>
#include <linux/etherdevice.h>
#include <linux/iopoll.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/unaligned.h>
#include <linux/xarray.h>

#include <net/flow_offload.h>
#include <net/dsa.h>
#include <net/dsa_zx279133_rtl8372n.h>
#include <net/pkt_cls.h>

#include "zx279133.h"

#define ZX279133_FAST_KEY_SIZE		16
#define ZX279133_FAST_DATA_SIZE		32
#define ZX279133_FAST_IKEY_SIZE		16
#define ZX279133_FAST_IKEY_DEPTH	4096
#define ZX279133_ZCAM_BLOCKS		4
#define ZX279133_ZCAM_CELLS		5
#define ZX279133_ZCAM_ADDRS		256
#define ZX279133_ZCAM_ENTRY_SIZE	64
#define ZX279133_ZCAM_SLOTS		(ZX279133_ZCAM_BLOCKS * \
					 ZX279133_ZCAM_CELLS * \
					 ZX279133_ZCAM_ADDRS)

#define ZX279133_SE_ALG_WR_DONE		0x50000
#define ZX279133_SE_ALG_WR_CMD		0x50004
#define ZX279133_SE_ALG_WR_DATA		0x50008
#define ZX279133_SE_HASH_AGCLK		BIT(2)
#define ZX279133_FAST_MULTI_SELECTOR	0x01
#define ZX279133_FAST_MULTI_FOOTER	0xc1
#define ZX279133_FAST_FULL_SELECTOR	0x02
#define ZX279133_FAST_FULL_FOOTER	0xe2
#define ZX279133_PPPOE_PUSH_WANID	0
#define ZX279133_PPPOE_POP_WANID	16
#define ZX279133_PPPOE_PUSH		1
#define ZX279133_PPPOE_POP		2

enum zx279133_flow_port {
	ZX279133_FLOW_PORT_NONE,
	ZX279133_FLOW_PORT_LAN,
	ZX279133_FLOW_PORT_WAN,
};

struct zx279133_flow_tuple {
	__be32 src_addr;
	__be32 dst_addr;
	__be16 src_port;
	__be16 dst_port;
};

struct zx279133_flow_data {
	struct ethhdr eth;
	struct zx279133_flow_tuple tuple;
	struct in6_addr src_v6;
	struct in6_addr dst_v6;
};

struct zx279133_flow_entry {
	u8 key[ZX279133_FAST_KEY_SIZE];
	u8 zblock;
	u8 zcell;
	u8 zaddr;
	u16 ikey;
	u16 age;
	u16 stat;
	u64 packets;
	u64 bytes;
	unsigned long lastused;
	bool has_stat;
	bool has_age;
	bool snat;
	bool pppoe;
};

struct zx279133_flow_offload {
	struct zx279133_eth *eth;
	/* Serializes software flow state and SE indirect accesses. */
	struct mutex lock;
	struct xarray flows;
	struct list_head block_cb_list;
	DECLARE_BITMAP(zcam_used, ZX279133_ZCAM_SLOTS);
	DECLARE_BITMAP(ikey_used, ZX279133_FAST_IKEY_DEPTH);
	DECLARE_BITMAP(age_used, ZX279133_FAST_AGE_DEPTH);
	DECLARE_BITMAP(stat_used, ZX279133_FAST_STAT_DEPTH);
	__be32 snat_addr;
	u32 snat_users;
	u32 saved_wanid_sip;
	u16 pppoe_sid;
	u16 saved_pppoe_sid[2];
	u32 pppoe_users;
	u8 saved_pppoe_mode[2];
};

struct zx279133_flow_block {
	struct zx279133_flow_offload *offload;
	struct net_device *ndev;
};

static unsigned int zx279133_zcam_slot(u8 zblock, u8 zcell, u8 zaddr)
{
	return (zblock * ZX279133_ZCAM_CELLS + zcell) *
		ZX279133_ZCAM_ADDRS + zaddr;
}

static int zx279133_fast_index_alloc(unsigned long *used,
				      unsigned int depth, u16 *index)
{
	unsigned long bit;

	bit = find_first_zero_bit(used, depth);
	if (bit == depth)
		return -ENOSPC;
	__set_bit(bit, used);
	*index = bit;

	return 0;
}

static u16 zx279133_crc16(u16 poly, const u8 *data, size_t len)
{
	u16 crc = 0;
	int bit;

	while (len--) {
		crc ^= (u16)data[len] << 8;
		for (bit = 0; bit < 8; bit++)
			crc = crc & BIT(15) ? (crc << 1) ^ poly : crc << 1;
	}

	return crc;
}

static int zx279133_zcam_alloc(struct zx279133_flow_offload *offload,
			       const u8 *key, u8 selector, u8 *zblock,
			       u8 *zcell, u8 *zaddr)
{
	u8 hash_input[49] = {};
	unsigned int slot;
	u16 crc;
	int block, cell;

	/* np.ko hashes the SDT selector, key, and zero response tail. */
	hash_input[0] = selector;
	memcpy(hash_input + 1, key, ZX279133_FAST_KEY_SIZE);
	for (block = 0; block < ZX279133_ZCAM_BLOCKS; block++) {
		crc = zx279133_crc16(block & 1 ? 0x8005 : 0x1021,
				     hash_input, sizeof(hash_input));
		for (cell = 0; cell < ZX279133_ZCAM_CELLS; cell++) {
			*zaddr = crc >> cell;
			slot = zx279133_zcam_slot(block, cell, *zaddr);
			if (test_and_set_bit(slot, offload->zcam_used))
				continue;
			*zblock = block;
			*zcell = cell;
			return 0;
		}
	}

	return -ENOSPC;
}

static int zx279133_zcam_write(struct zx279133_flow_offload *offload,
			       u8 zblock, u8 zcell, u8 zaddr,
			       const u32 *table, unsigned int words)
{
	struct zx279133_eth *eth = offload->eth;
	u32 agclk, cmd, status;
	int i, ret;

	agclk = readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG);
	if (agclk & ZX279133_SE_HASH_AGCLK)
		writel(agclk & ~ZX279133_SE_HASH_AGCLK,
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	ret = readl_poll_timeout_atomic(eth->pps_base + ZX279133_SE_ALG_WR_DONE,
					status, status & BIT(0), 1, 1000);
	if (ret)
		goto out_restore_clock;

	for (i = 0; i < words; i++)
		writel(table[i], eth->pps_base + ZX279133_SE_ALG_WR_DATA + 4 * i);

	cmd = zaddr | (zblock >> 1) << 12 | (zblock & 1) << 11 |
		zcell << 8 | ((BIT(words / 4) - 1) & 0xf) << 16;
	writel(cmd, eth->pps_base + ZX279133_SE_ALG_WR_CMD);

out_restore_clock:
	if (agclk & ZX279133_SE_HASH_AGCLK)
		writel(agclk, eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	return ret;
}

static void zx279133_put_ipv4(u8 *dst, __be32 addr)
{
	put_unaligned(be32_to_cpu(addr), (u32 *)dst);
}

static void zx279133_fast_key_build(u8 *key,
				    const struct zx279133_flow_tuple *tuple,
				    u8 ip_proto)
{
	memset(key, 0, ZX279133_FAST_KEY_SIZE);
	put_unaligned(ntohs(tuple->dst_port), (u16 *)(key + 3));
	put_unaligned(ntohs(tuple->src_port), (u16 *)(key + 5));
	zx279133_put_ipv4(key + 7, tuple->dst_addr);
	zx279133_put_ipv4(key + 11, tuple->src_addr);
	key[15] = ip_proto;
}

#define ZX279133_IPV6_CRC_POLY		0x04c11db7

static u32 zx279133_ipv6_crc32(u32 crc, const u8 *data, size_t len)
{
	int bit;

	while (len--) {
		crc ^= (u32)*data++ << 24;
		for (bit = 0; bit < 8; bit++)
			crc = crc & BIT(31) ?
				(crc << 1) ^ ZX279133_IPV6_CRC_POLY : crc << 1;
	}

	return crc;
}

static void zx279133_fast_key_build_v6(u8 *key,
				       const struct zx279133_flow_data *data,
				       u8 ip_proto)
{
	u32 crc;

	memset(key, 0, ZX279133_FAST_KEY_SIZE);
	put_unaligned(ntohs(data->tuple.dst_port), (u16 *)(key + 3));
	put_unaligned(ntohs(data->tuple.src_port), (u16 *)(key + 5));

	/* SPA full-tuple mode covers destination then source for DIP32 and
	 * source then destination for SIP32, using big-endian CRC-32.
	 */
	crc = zx279133_ipv6_crc32(0, data->dst_v6.s6_addr, sizeof(data->dst_v6));
	crc = zx279133_ipv6_crc32(crc, data->src_v6.s6_addr, sizeof(data->src_v6));
	put_unaligned(crc, (u32 *)(key + 7));
	crc = zx279133_ipv6_crc32(0, data->src_v6.s6_addr, sizeof(data->src_v6));
	crc = zx279133_ipv6_crc32(crc, data->dst_v6.s6_addr, sizeof(data->dst_v6));
	put_unaligned(crc, (u32 *)(key + 11));
	key[15] = ip_proto;
}

static void zx279133_fast_response_build(u8 *fast,
					 const struct zx279133_flow_data *data,
					 const struct zx279133_flow_tuple *xlate,
					 enum zx279133_flow_port ingress,
					 u16 lan_vid, u16 ikey, u16 stat,
					 bool has_stat, bool pppoe)
{
	int i;

	/* The response stores MAC addresses and IPv4 values least-significant
	 * byte first, exactly as np.ko's fast_hashinfo_set() emits them.
	 */
	memset(fast, 0, ZX279133_FAST_DATA_SIZE);
	if (pppoe)
		put_unaligned(4, (u32 *)(fast + 16));
	put_unaligned(stat, (u16 *)(fast + 2));
	for (i = 0; i < ETH_ALEN; i++)
		fast[8 + i] = data->eth.h_dest[ETH_ALEN - 1 - i];

	put_unaligned(ingress == ZX279133_FLOW_PORT_LAN ? zx279133_tx_port :
		      lan_vid, (u16 *)(fast + 20));
	fast[23] = ingress == ZX279133_FLOW_PORT_WAN ? 16 : 0;
	put_unaligned(ikey, (u16 *)(fast + 26));
	fast[29] = ingress == ZX279133_FLOW_PORT_LAN ? BIT(1) : 0;
	if (has_stat)
		fast[29] |= BIT(4);
	fast[30] = BIT(5) | BIT(4) | BIT(3);

	if (xlate->src_addr != data->tuple.src_addr)
		fast[29] |= BIT(7);
	if (xlate->dst_addr != data->tuple.dst_addr) {
		zx279133_put_ipv4(fast + 4, xlate->dst_addr);
		fast[30] |= BIT(0);
	}
	if (ingress == ZX279133_FLOW_PORT_LAN) {
		put_unaligned(ntohs(xlate->src_port), (u16 *)fast);
		fast[30] |= BIT(1);
	}
	if (ingress == ZX279133_FLOW_PORT_WAN) {
		put_unaligned(ntohs(xlate->dst_port), (u16 *)(fast + 14));
		fast[30] |= BIT(2);
	}
}

static void zx279133_fast_multi_table_build(u32 *table, const u8 *fast,
					    const u8 *key, u16 age)
{
	u8 packed[18] = {};
	u8 *bytes = (u8 *)table;
	u32 tail;
	int i;

	/* fast_table_write() packs response bytes 17..31 around the age index. */
	for (i = 0; i < 14; i++) {
		packed[i] |= (fast[17 + i] & 0x3) << 6;
		packed[i + 1] |= fast[17 + i] >> 2;
	}
	tail = (u32)age << 8 | (fast[31] & 0x3) << 6 |
	       (u32)(fast[31] & ~0x3) << 24;
	packed[14] |= tail;
	packed[15] = tail >> 8;
	packed[16] = tail >> 16;
	packed[17] = tail >> 24;

	memset(table, 0, ZX279133_ZCAM_ENTRY_SIZE / 2);
	memcpy(bytes, packed + 3, 15);
	memcpy(bytes + 15, key, ZX279133_FAST_KEY_SIZE);
	bytes[31] = ZX279133_FAST_MULTI_FOOTER;
}

static void zx279133_fast_full_table_build(u32 *table, const u8 *fast,
					   const u8 *key)
{
	u8 *bytes = (u8 *)table;

	/* SDT14 is a 512-bit single-hash table.  With age/AS disabled its
	 * 32-byte response is stored intact ahead of the key, retaining the
	 * len_changed field that SDT43's compact response omits.
	 */
	memset(table, 0, ZX279133_ZCAM_ENTRY_SIZE);
	memcpy(bytes + 15, fast, ZX279133_FAST_DATA_SIZE);
	memcpy(bytes + 47, key, ZX279133_FAST_KEY_SIZE);
	bytes[63] = ZX279133_FAST_FULL_FOOTER;
}

static void zx279133_flow_mangle_eth(const struct flow_action_entry *act,
				     struct ethhdr *eth)
{
	u8 *dest = (u8 *)eth + act->mangle.offset;
	const u8 *src = (const u8 *)&act->mangle.val;

	if (act->mangle.offset > 8)
		return;
	if (act->mangle.mask == 0xffff) {
		src += 2;
		dest += 2;
	}
	memcpy(dest, src, act->mangle.mask ? 2 : 4);
}

static int zx279133_flow_mangle_ports(const struct flow_action_entry *act,
				      struct zx279133_flow_tuple *tuple)
{
	u32 val = ntohl(act->mangle.val);

	switch (act->mangle.offset) {
	case 0:
		if (act->mangle.mask == ~htonl(0xffff))
			tuple->dst_port = cpu_to_be16(val);
		else
			tuple->src_port = cpu_to_be16(val >> 16);
		break;
	case 2:
		tuple->dst_port = cpu_to_be16(val);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int zx279133_flow_mangle_ipv4(const struct flow_action_entry *act,
				     struct zx279133_flow_tuple *tuple)
{
	__be32 *addr;

	switch (act->mangle.offset) {
	case offsetof(struct iphdr, saddr):
		addr = &tuple->src_addr;
		break;
	case offsetof(struct iphdr, daddr):
		addr = &tuple->dst_addr;
		break;
	default:
		return -EOPNOTSUPP;
	}
	memcpy(addr, &act->mangle.val, sizeof(*addr));

	return 0;
}

static enum zx279133_flow_port
zx279133_flow_dev_port(struct zx279133_eth *eth, const struct net_device *dev)
{
	if (!dev)
		return ZX279133_FLOW_PORT_NONE;
	if (dev == eth->ndev)
		return ZX279133_FLOW_PORT_WAN;
	if (eth->lan_ndev &&
	    (dev == eth->lan_ndev || dev_get_iflink(dev) == eth->lan_ndev->ifindex))
		return ZX279133_FLOW_PORT_LAN;
	return ZX279133_FLOW_PORT_NONE;
}

static int zx279133_flow_lan_vid(struct zx279133_eth *eth,
				 struct net_device *dev, u16 *vid)
{
#if IS_REACHABLE(CONFIG_NET_DSA)
	struct dsa_port *dp = dsa_port_from_netdev(dev);

	if (IS_ERR(dp) || dsa_port_to_conduit(dp) != eth->lan_ndev ||
	    dp->index < ZX279133_RTL8372N_USER_PORT_MIN ||
	    dp->index > ZX279133_RTL8372N_USER_PORT_MAX)
		return -EOPNOTSUPP;

	*vid = ZX279133_RTL8372N_TRANSPORT_VID_BASE + dp->index;
	return 0;
#else
	return -EOPNOTSUPP;
#endif
}

static int zx279133_flow_snat_get(struct zx279133_flow_offload *offload,
				  __be32 addr)
{
	int ret;

	if (offload->snat_users) {
		if (offload->snat_addr != addr)
			return -EOPNOTSUPP;
		offload->snat_users++;
		return 0;
	}

	ret = zx279133_program_wanid_sip(offload->eth, 0,
					 be32_to_cpu(addr),
					 &offload->saved_wanid_sip);
	if (ret)
		return ret;
	offload->snat_addr = addr;
	offload->snat_users = 1;
	return 0;
}

static void zx279133_flow_snat_put(struct zx279133_flow_offload *offload)
{
	if (--offload->snat_users)
		return;
	if (zx279133_program_wanid_sip(offload->eth, 0,
				       offload->saved_wanid_sip, NULL))
		dev_warn(offload->eth->dev, "failed to restore WANID source IP\n");
	offload->snat_addr = 0;
}

static int zx279133_flow_pppoe_get(struct zx279133_flow_offload *offload,
				   u16 sid)
{
	int ret;

	if (offload->pppoe_users) {
		if (offload->pppoe_sid != sid)
			return -EOPNOTSUPP;
		offload->pppoe_users++;
		return 0;
	}

	ret = zx279133_program_wanid_pppoe(offload->eth,
					   ZX279133_PPPOE_PUSH_WANID,
					   ZX279133_PPPOE_PUSH, sid,
					   &offload->saved_pppoe_mode[0],
					   &offload->saved_pppoe_sid[0]);
	if (ret)
		return ret;
	ret = zx279133_program_wanid_pppoe(offload->eth,
					   ZX279133_PPPOE_POP_WANID,
					   ZX279133_PPPOE_POP, 0,
					   &offload->saved_pppoe_mode[1],
					   &offload->saved_pppoe_sid[1]);
	if (ret) {
		zx279133_program_wanid_pppoe(offload->eth,
					     ZX279133_PPPOE_PUSH_WANID,
					     offload->saved_pppoe_mode[0],
					     offload->saved_pppoe_sid[0],
					     NULL, NULL);
		return ret;
	}
	offload->pppoe_sid = sid;
	offload->pppoe_users = 1;

	return 0;
}

static void zx279133_flow_pppoe_put(struct zx279133_flow_offload *offload)
{
	int ret;

	if (--offload->pppoe_users)
		return;
	ret = zx279133_program_wanid_pppoe(offload->eth,
					   ZX279133_PPPOE_POP_WANID,
					   offload->saved_pppoe_mode[1],
					   offload->saved_pppoe_sid[1],
					   NULL, NULL);
	ret |= zx279133_program_wanid_pppoe(offload->eth,
					    ZX279133_PPPOE_PUSH_WANID,
					    offload->saved_pppoe_mode[0],
					    offload->saved_pppoe_sid[0],
					    NULL, NULL);
	if (ret)
		dev_warn(offload->eth->dev, "failed to restore WANID PPPoE state\n");
	offload->pppoe_sid = 0;
}

static int
zx279133_flow_parse(struct zx279133_flow_offload *offload,
		    struct net_device *bind_dev, struct flow_cls_offload *f,
		    struct zx279133_flow_data *data,
		    struct zx279133_flow_tuple *xlate,
		    enum zx279133_flow_port *ingress, u16 *lan_vid,
		    u8 *ip_proto, u16 *pppoe_sid, bool *ipv6)
{
	struct flow_rule *rule = flow_cls_offload_flow_rule(f);
	struct flow_match_ipv4_addrs addrs;
	struct flow_match_ipv6_addrs addrs6;
	struct flow_match_control control;
	struct flow_match_basic basic;
	struct flow_match_ports ports;
	struct flow_match_meta meta;
	struct flow_action_entry *act;
	struct net_device *idev;
	enum zx279133_flow_port egress;
	struct net_device *odev = NULL;
	int i, ret;

	if (!flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_CONTROL) ||
	    !flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_BASIC) ||
	    (!flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_IPV4_ADDRS) &&
	     !flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_IPV6_ADDRS)) ||
	    !flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_PORTS))
		return -EOPNOTSUPP;

	idev = bind_dev;
	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_META)) {
		flow_rule_match_meta(rule, &meta);
		rcu_read_lock();
		idev = dev_get_by_index_rcu(dev_net(bind_dev),
					    meta.key->ingress_ifindex);
		*ingress = zx279133_flow_dev_port(offload->eth, idev);
		rcu_read_unlock();
	} else {
		*ingress = zx279133_flow_dev_port(offload->eth, idev);
	}
	if (*ingress == ZX279133_FLOW_PORT_NONE)
		return -EOPNOTSUPP;

	flow_rule_match_control(rule, &control);
	if (flow_rule_has_control_flags(control.mask->flags, f->common.extack))
		return -EOPNOTSUPP;
	switch (control.key->addr_type) {
	case FLOW_DISSECTOR_KEY_IPV4_ADDRS:
		*ipv6 = false;
		break;
	case FLOW_DISSECTOR_KEY_IPV6_ADDRS:
		*ipv6 = true;
		break;
	default:
		return -EOPNOTSUPP;
	}

	flow_rule_match_basic(rule, &basic);
	if (basic.key->n_proto != htons(*ipv6 ? ETH_P_IPV6 : ETH_P_IP) ||
	    basic.mask->n_proto != htons(0xffff) ||
	    basic.mask->ip_proto != 0xff ||
	    (basic.key->ip_proto != IPPROTO_TCP &&
	     basic.key->ip_proto != IPPROTO_UDP))
		return -EOPNOTSUPP;
	*ip_proto = basic.key->ip_proto;

	if (*ipv6) {
		flow_rule_match_ipv6_addrs(rule, &addrs6);
		if (memchr_inv(&addrs6.mask->src, 0xff,
			       sizeof(addrs6.mask->src)) ||
		    memchr_inv(&addrs6.mask->dst, 0xff,
			       sizeof(addrs6.mask->dst)))
			return -EOPNOTSUPP;
		data->src_v6 = addrs6.key->src;
		data->dst_v6 = addrs6.key->dst;
	} else {
		flow_rule_match_ipv4_addrs(rule, &addrs);
		if (addrs.mask->src != htonl(~0U) ||
		    addrs.mask->dst != htonl(~0U))
			return -EOPNOTSUPP;
		data->tuple.src_addr = addrs.key->src;
		data->tuple.dst_addr = addrs.key->dst;
	}

	flow_rule_match_ports(rule, &ports);
	if (ports.mask->src != htons(0xffff) ||
	    ports.mask->dst != htons(0xffff))
		return -EOPNOTSUPP;
	data->tuple.src_port = ports.key->src;
	data->tuple.dst_port = ports.key->dst;
	*xlate = data->tuple;

	flow_action_for_each(i, act, &rule->action) {
		switch (act->id) {
		case FLOW_ACTION_MANGLE:
			if (act->mangle.htype == FLOW_ACT_MANGLE_HDR_TYPE_ETH)
				zx279133_flow_mangle_eth(act, &data->eth);
			break;
		case FLOW_ACTION_REDIRECT:
			odev = act->dev;
			break;
		case FLOW_ACTION_CSUM:
			break;
		case FLOW_ACTION_PPPOE_PUSH:
			if (*pppoe_sid || !act->pppoe.sid)
				return -EOPNOTSUPP;
			*pppoe_sid = act->pppoe.sid;
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	if (!odev || !is_valid_ether_addr(data->eth.h_source) ||
	    !is_valid_ether_addr(data->eth.h_dest) ||
	    !ether_addr_equal(data->eth.h_source, offload->eth->ndev->dev_addr))
		return -EOPNOTSUPP;
	egress = zx279133_flow_dev_port(offload->eth, odev);
	if ((*ingress == ZX279133_FLOW_PORT_LAN &&
	     egress != ZX279133_FLOW_PORT_WAN) ||
	    (*ingress == ZX279133_FLOW_PORT_WAN &&
	     egress != ZX279133_FLOW_PORT_LAN))
		return -EOPNOTSUPP;
	if (*pppoe_sid && *ingress != ZX279133_FLOW_PORT_LAN)
		return -EOPNOTSUPP;
	if (*ingress == ZX279133_FLOW_PORT_WAN) {
		ret = zx279133_flow_lan_vid(offload->eth, odev, lan_vid);
		if (ret)
			return ret;
	} else {
		*lan_vid = 0;
	}

	flow_action_for_each(i, act, &rule->action) {
		if (act->id != FLOW_ACTION_MANGLE)
			continue;
		switch (act->mangle.htype) {
		case FLOW_ACT_MANGLE_HDR_TYPE_ETH:
			ret = 0;
			break;
		case FLOW_ACT_MANGLE_HDR_TYPE_IP4:
			ret = *ipv6 ? -EOPNOTSUPP :
				zx279133_flow_mangle_ipv4(act, xlate);
			break;
		case FLOW_ACT_MANGLE_HDR_TYPE_TCP:
		case FLOW_ACT_MANGLE_HDR_TYPE_UDP:
			ret = zx279133_flow_mangle_ports(act, xlate);
			break;
		default:
			ret = -EOPNOTSUPP;
		}
		if (ret)
			return ret;
	}

	return 0;
}

static bool zx279133_flow_key_exists(struct zx279133_flow_offload *offload,
				     const u8 *key)
{
	struct zx279133_flow_entry *entry;
	unsigned long index;

	xa_for_each(&offload->flows, index, entry)
		if (!memcmp(entry->key, key, ZX279133_FAST_KEY_SIZE))
			return true;
	return false;
}

static int zx279133_flow_replace(struct zx279133_flow_offload *offload,
				 struct net_device *bind_dev,
				 struct flow_cls_offload *f)
{
	struct zx279133_flow_tuple xlate;
	struct zx279133_flow_data data = {};
	struct zx279133_flow_entry *entry;
	enum zx279133_flow_port ingress;
	u8 fast[ZX279133_FAST_DATA_SIZE];
	u32 ikey[ZX279133_FAST_IKEY_SIZE / sizeof(u32)];
	u32 table[ZX279133_ZCAM_ENTRY_SIZE / sizeof(u32)];
	bool ipv6, pppoe, snat;
	u16 lan_vid, pppoe_sid = 0;
	u8 ip_proto;
	int ret;

	ret = zx279133_flow_parse(offload, bind_dev, f, &data, &xlate,
				  &ingress, &lan_vid, &ip_proto, &pppoe_sid,
				  &ipv6);
	if (ret)
		return ret;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;
	if (ipv6)
		zx279133_fast_key_build_v6(entry->key, &data, ip_proto);
	else
		zx279133_fast_key_build(entry->key, &data.tuple, ip_proto);
	snat = !ipv6 && xlate.src_addr != data.tuple.src_addr;
	entry->snat = snat;

	mutex_lock(&offload->lock);
	if (!READ_ONCE(offload->eth->hardware_prepared)) {
		ret = -ENETDOWN;
		goto out_free;
	}
	if (xa_load(&offload->flows, f->cookie) ||
	    zx279133_flow_key_exists(offload, entry->key)) {
		ret = -EEXIST;
		goto out_free;
	}
	if (!pppoe_sid && ingress == ZX279133_FLOW_PORT_LAN &&
	    offload->pppoe_users) {
		ret = -EOPNOTSUPP;
		goto out_free;
	}
	pppoe = pppoe_sid || (ingress == ZX279133_FLOW_PORT_WAN &&
				      offload->pppoe_users);
	entry->pppoe = pppoe;
	entry->has_age = !pppoe;
	ret = zx279133_zcam_alloc(offload, entry->key,
				  pppoe ? ZX279133_FAST_FULL_SELECTOR :
					   ZX279133_FAST_MULTI_SELECTOR,
				  &entry->zblock, &entry->zcell,
				  &entry->zaddr);
	if (ret)
		goto out_free;
	ret = zx279133_fast_index_alloc(offload->ikey_used,
					ZX279133_FAST_IKEY_DEPTH, &entry->ikey);
	if (ret)
		goto out_release_slot;
	ret = zx279133_fast_index_alloc(offload->age_used,
					ZX279133_FAST_AGE_DEPTH, &entry->age);
	if (ret)
		goto out_release_ikey;
	ret = zx279133_fast_index_alloc(offload->stat_used,
					ZX279133_FAST_STAT_DEPTH, &entry->stat);
	if (!ret) {
		entry->has_stat = true;
		ret = zx279133_fast_stats_read(offload->eth, entry->stat,
					       &entry->packets, &entry->bytes);
		if (ret)
			goto out_release_stat;
	} else if (ret != -ENOSPC) {
		goto out_release_age;
	}
	if (snat) {
		ret = zx279133_flow_snat_get(offload, xlate.src_addr);
		if (ret)
			goto out_release_stat;
	}
	if (pppoe) {
		u16 sid = pppoe_sid ?: offload->pppoe_sid;

		ret = zx279133_flow_pppoe_get(offload, sid);
		if (ret)
			goto out_put_snat;
	}
	zx279133_fast_response_build(fast, &data, &xlate, ingress, lan_vid,
				     entry->ikey, entry->stat,
				     entry->has_stat, pppoe);
	if (pppoe) {
		zx279133_fast_full_table_build(table, fast, entry->key);
	} else {
		memcpy(ikey, fast, sizeof(ikey));
		zx279133_fast_multi_table_build(table, fast, entry->key, entry->age);
		ret = zx279133_fast_ikey_write(offload->eth, entry->ikey, ikey);
		if (ret)
			goto out_put_pppoe;
	}
	ret = xa_err(xa_store(&offload->flows, f->cookie, entry, GFP_KERNEL));
	if (ret)
		goto out_clear_ikey;
	ret = zx279133_zcam_write(offload, entry->zblock, entry->zcell,
				  entry->zaddr, table,
				  pppoe ? ARRAY_SIZE(table) : ARRAY_SIZE(ikey) * 2);
	if (ret) {
		xa_erase(&offload->flows, f->cookie);
		goto out_clear_ikey;
	}
	mutex_unlock(&offload->lock);
	return 0;

out_clear_ikey:
	if (!pppoe) {
		memset(ikey, 0, sizeof(ikey));
		zx279133_fast_ikey_write(offload->eth, entry->ikey, ikey);
	}
out_put_pppoe:
	if (entry->pppoe)
		zx279133_flow_pppoe_put(offload);
out_put_snat:
	if (snat)
		zx279133_flow_snat_put(offload);
out_release_stat:
	if (entry->has_stat)
		__clear_bit(entry->stat, offload->stat_used);
out_release_age:
	__clear_bit(entry->age, offload->age_used);
out_release_ikey:
	__clear_bit(entry->ikey, offload->ikey_used);
out_release_slot:
	clear_bit(zx279133_zcam_slot(entry->zblock, entry->zcell,
				     entry->zaddr), offload->zcam_used);
out_free:
	mutex_unlock(&offload->lock);
	kfree(entry);
	return ret;
}

static int
zx279133_flow_entry_hw_clear(struct zx279133_flow_offload *offload,
			     const struct zx279133_flow_entry *entry)
{
	u32 ikey[ZX279133_FAST_IKEY_SIZE / sizeof(u32)] = {};
	u32 table[ZX279133_ZCAM_ENTRY_SIZE / sizeof(u32)] = {};
	int ret;

	ret = zx279133_zcam_write(offload, entry->zblock, entry->zcell,
				  entry->zaddr, table,
				  entry->pppoe ? ARRAY_SIZE(table) :
						 ARRAY_SIZE(ikey) * 2);
	if (ret)
		return ret;
	if (!entry->pppoe) {
		ret = zx279133_fast_ikey_write(offload->eth, entry->ikey, ikey);
		if (ret)
			return ret;
	}

	return 0;
}

static int zx279133_flow_destroy(struct zx279133_flow_offload *offload,
				 struct flow_cls_offload *f)
{
	struct zx279133_flow_entry *entry;
	int ret;

	mutex_lock(&offload->lock);
	entry = xa_load(&offload->flows, f->cookie);
	if (!entry) {
		ret = -ENOENT;
		goto out_unlock;
	}
	ret = zx279133_flow_entry_hw_clear(offload, entry);
	if (ret)
		goto out_unlock;
	xa_erase(&offload->flows, f->cookie);
	clear_bit(zx279133_zcam_slot(entry->zblock, entry->zcell,
				     entry->zaddr), offload->zcam_used);
	__clear_bit(entry->ikey, offload->ikey_used);
	__clear_bit(entry->age, offload->age_used);
	if (entry->has_stat)
		__clear_bit(entry->stat, offload->stat_used);
	if (entry->pppoe)
		zx279133_flow_pppoe_put(offload);
	if (entry->snat)
		zx279133_flow_snat_put(offload);
	kfree(entry);

out_unlock:
	mutex_unlock(&offload->lock);
	return ret;
}

static int zx279133_flow_stats(struct zx279133_flow_offload *offload,
			       struct flow_cls_offload *f)
{
	struct zx279133_flow_entry *entry;
	unsigned long lastused;
	bool used;
	u64 packets, bytes;
	int ret;

	mutex_lock(&offload->lock);
	entry = xa_load(&offload->flows, f->cookie);
	if (!entry) {
		ret = -ENOENT;
		goto out_unlock;
	}
	if (entry->has_age) {
		ret = zx279133_fast_age_read_clear(offload->eth, entry->age, &used);
		if (ret)
			goto out_unlock;
		if (used)
			entry->lastused = jiffies;
	}
	packets = entry->packets;
	bytes = entry->bytes;
	if (entry->has_stat) {
		ret = zx279133_fast_stats_read(offload->eth, entry->stat,
					       &packets, &bytes);
		if (ret)
			goto out_unlock;
	}
	if (!entry->has_age &&
	    (packets != entry->packets || bytes != entry->bytes))
		entry->lastused = jiffies;
	lastused = entry->lastused;
	flow_stats_update(&f->stats, bytes - entry->bytes,
			  packets - entry->packets, 0, lastused,
			  FLOW_ACTION_HW_STATS_DELAYED);
	entry->packets = packets;
	entry->bytes = bytes;

out_unlock:
	mutex_unlock(&offload->lock);
	return ret;
}

static int zx279133_flow_command(struct zx279133_flow_offload *offload,
				 struct net_device *bind_dev,
				 struct flow_cls_offload *f)
{
	switch (f->command) {
	case FLOW_CLS_REPLACE:
		return zx279133_flow_replace(offload, bind_dev, f);
	case FLOW_CLS_DESTROY:
		return zx279133_flow_destroy(offload, f);
	case FLOW_CLS_STATS:
		return zx279133_flow_stats(offload, f);
	default:
		return -EOPNOTSUPP;
	}
}

static int zx279133_flow_block_cb(enum tc_setup_type type, void *type_data,
				  void *cb_priv)
{
	struct zx279133_flow_block *block = cb_priv;
	struct flow_cls_offload *f = type_data;

	if (type != TC_SETUP_CLSFLOWER)
		return -EOPNOTSUPP;
	return zx279133_flow_command(block->offload, block->ndev, f);
}

static void zx279133_flow_block_release(void *cb_priv)
{
	kfree(cb_priv);
}

static void
zx279133_flow_entries_clear(struct zx279133_flow_offload *offload,
			    bool clear_hardware)
{
	struct zx279133_flow_entry *entry;
	unsigned long index;

	mutex_lock(&offload->lock);
	xa_for_each(&offload->flows, index, entry) {
		if (clear_hardware &&
		    zx279133_flow_entry_hw_clear(offload, entry))
			dev_warn(offload->eth->dev,
				 "failed to clear hardware flow %lu\n", index);
		if (entry->pppoe)
			zx279133_flow_pppoe_put(offload);
		if (entry->snat)
			zx279133_flow_snat_put(offload);
		kfree(entry);
	}
	xa_destroy(&offload->flows);
	xa_init(&offload->flows);
	bitmap_zero(offload->zcam_used, ZX279133_ZCAM_SLOTS);
	bitmap_zero(offload->ikey_used, ZX279133_FAST_IKEY_DEPTH);
	bitmap_zero(offload->age_used, ZX279133_FAST_AGE_DEPTH);
	bitmap_zero(offload->stat_used, ZX279133_FAST_STAT_DEPTH);
	mutex_unlock(&offload->lock);
}

static int zx279133_flow_setup_block(struct zx279133_flow_offload *offload,
				     struct net_device *ndev,
				     struct flow_block_offload *f)
{
	struct zx279133_flow_block *block;
	struct flow_block_cb *block_cb;
	flow_setup_cb_t *cb = zx279133_flow_block_cb;

	if (!tc_can_offload(ndev) ||
	    f->binder_type != FLOW_BLOCK_BINDER_TYPE_CLSACT_INGRESS)
		return -EOPNOTSUPP;
	f->driver_block_list = &offload->block_cb_list;

	switch (f->command) {
	case FLOW_BLOCK_BIND:
		block_cb = flow_block_cb_lookup(f->block, cb, ndev);
		if (block_cb) {
			flow_block_cb_incref(block_cb);
			return 0;
		}
		block = kzalloc(sizeof(*block), GFP_KERNEL);
		if (!block)
			return -ENOMEM;
		block->offload = offload;
		block->ndev = ndev;
		block_cb = flow_block_cb_alloc(cb, ndev, block,
					       zx279133_flow_block_release);
		if (IS_ERR(block_cb)) {
			kfree(block);
			return PTR_ERR(block_cb);
		}
		flow_block_cb_incref(block_cb);
		flow_block_cb_add(block_cb, f);
		list_add_tail(&block_cb->driver_list, &offload->block_cb_list);
		return 0;
	case FLOW_BLOCK_UNBIND:
		block_cb = flow_block_cb_lookup(f->block, cb, ndev);
		if (!block_cb)
			return -ENOENT;
		if (!flow_block_cb_decref(block_cb)) {
			flow_block_cb_remove(block_cb, f);
			list_del(&block_cb->driver_list);
			if (list_empty(&offload->block_cb_list))
				zx279133_flow_entries_clear(offload, true);
		}
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

int zx279133_flow_offload_setup_tc(struct zx279133_eth *eth,
				   struct net_device *ndev,
				    enum tc_setup_type type, void *type_data)
{
	if (type != TC_SETUP_BLOCK && type != TC_SETUP_FT)
		return -EOPNOTSUPP;
	return zx279133_flow_setup_block(eth->flow_offload, ndev, type_data);
}

void zx279133_flow_offload_flush(struct zx279133_eth *eth)
{
	zx279133_flow_entries_clear(eth->flow_offload, false);
}

static void zx279133_flow_offload_cleanup(void *data)
{
	struct zx279133_flow_offload *offload = data;

	zx279133_flow_entries_clear(offload, false);
	offload->eth->flow_offload = NULL;
}

int zx279133_flow_offload_init(struct zx279133_eth *eth)
{
	struct zx279133_flow_offload *offload;

	offload = devm_kzalloc(eth->dev, sizeof(*offload), GFP_KERNEL);
	if (!offload)
		return -ENOMEM;
	offload->eth = eth;
	mutex_init(&offload->lock);
	xa_init(&offload->flows);
	INIT_LIST_HEAD(&offload->block_cb_list);
	eth->flow_offload = offload;

	return devm_add_action_or_reset(eth->dev,
					zx279133_flow_offload_cleanup, offload);
}
