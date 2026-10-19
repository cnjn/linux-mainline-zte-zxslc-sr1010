// SPDX-License-Identifier: GPL-2.0-only
/*
 * mcfast: forward routed IPv4 multicast at tc ingress.
 *
 * For packets matching a resolved kernel multicast route the program does
 * what ip_mr_forward() would (TTL check and decrement, Ethernet header for
 * the egress) and redirects to each egress device, typically a bridge, which
 * still applies its own snooping state. Everything else continues to the
 * next program and the regular stack.
 */
#define KBUILD_MODNAME "mcfast"
#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/pkt_cls.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#include "mcfast.h"

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MCFAST_MAX_ROUTES);
	__type(key, struct mcfast_key);
	__type(value, struct mcfast_route);
} mcfast_routes SEC(".maps");

static __always_inline struct mcfast_route *
mcfast_lookup(__u32 ifindex, struct iphdr *ip)
{
	struct mcfast_key key = {
		.ifindex = ifindex,
		.src = ip->saddr,
		.grp = ip->daddr,
	};
	struct mcfast_route *route;

	/* Link-local groups are never routed. */
	if ((ip->daddr & bpf_htonl(0xf0000000)) != bpf_htonl(0xe0000000) ||
	    (ip->daddr & bpf_htonl(0xffffff00)) == bpf_htonl(0xe0000000))
		return NULL;

	route = bpf_map_lookup_elem(&mcfast_routes, &key);
	if (route)
		return route;
	key.src = 0;
	return bpf_map_lookup_elem(&mcfast_routes, &key);
}

/*
 * Called with skb->data at the Ethernet header, already carrying the group
 * destination address. Returns a tc verdict.
 */
static __always_inline int
mcfast_xmit(struct __sk_buff *skb, struct mcfast_route *route)
{
	__u32 n = route->n_oifs;
	int i;

	__sync_fetch_and_add(&route->packets, 1);
	__sync_fetch_and_add(&route->bytes, skb->len - ETH_HLEN);

	for (i = 0; i < MCFAST_MAX_OIFS; i++) {
		if (i >= n)
			break;
		bpf_skb_store_bytes(skb, ETH_ALEN, route->oif[i].mac, ETH_ALEN,
				    0);
		if (i == n - 1)
			return bpf_redirect(route->oif[i].ifindex, 0);
		bpf_clone_redirect(skb, route->oif[i].ifindex, 0);
	}

	return TC_ACT_SHOT;
}

/* Decrement the TTL at offset @l3 and fix up the header checksum. */
static __always_inline int
mcfast_ttl_dec(struct __sk_buff *skb, struct iphdr *ip, __u32 l3)
{
	__u16 old = *(__u16 *)&ip->ttl, new;

	ip->ttl--;
	new = *(__u16 *)&ip->ttl;
	return bpf_l3_csum_replace(skb, l3 + __builtin_offsetof(struct iphdr, check),
				   old, new, sizeof(__u16));
}

static __always_inline void
mcfast_group_mac(__u8 *mac, __be32 grp)
{
	__u32 g = bpf_ntohl(grp);

	mac[0] = 0x01;
	mac[1] = 0x00;
	mac[2] = 0x5e;
	mac[3] = (g >> 16) & 0x7f;
	mac[4] = g >> 8;
	mac[5] = g;
}

static __always_inline bool
mcfast_check(struct __sk_buff *skb, struct iphdr *ip, __u32 l3,
	     struct mcfast_route *route)
{
	if (ip->version != 4 || ip->ttl <= route->ttl_min)
		return false;
	return skb->len - l3 <= route->mtu;
}

/* Ethernet ingress, e.g. an IPoE WAN. */
SEC("tcx/ingress")
int mcfast_eth(struct __sk_buff *skb)
{
	void *data = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;
	struct ethhdr *eth = data;
	struct iphdr *ip = data + ETH_HLEN;
	struct mcfast_route *route;

	if ((void *)(ip + 1) > data_end || eth->h_proto != bpf_htons(ETH_P_IP))
		return TC_ACT_UNSPEC;
	route = mcfast_lookup(skb->ifindex, ip);
	if (!route || !mcfast_check(skb, ip, ETH_HLEN, route))
		return TC_ACT_UNSPEC;

	mcfast_group_mac(eth->h_dest, ip->daddr);
	if (mcfast_ttl_dec(skb, ip, ETH_HLEN))
		return TC_ACT_SHOT;
	return mcfast_xmit(skb, route);
}

/* Ingress without a link-layer header, e.g. a PPPoE session device. */
SEC("tcx/ingress")
int mcfast_ip(struct __sk_buff *skb)
{
	void *data = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;
	struct iphdr *ip = data;
	struct mcfast_route *route;
	struct ethhdr eth = {
		.h_proto = bpf_htons(ETH_P_IP),
	};

	if ((void *)(ip + 1) > data_end ||
	    skb->protocol != bpf_htons(ETH_P_IP))
		return TC_ACT_UNSPEC;
	route = mcfast_lookup(skb->ifindex, ip);
	if (!route || !mcfast_check(skb, ip, 0, route))
		return TC_ACT_UNSPEC;

	mcfast_group_mac(eth.h_dest, ip->daddr);
	if (mcfast_ttl_dec(skb, ip, 0) ||
	    bpf_skb_change_head(skb, ETH_HLEN, 0) ||
	    bpf_skb_store_bytes(skb, 0, &eth, sizeof(eth), 0))
		return TC_ACT_SHOT;
	return mcfast_xmit(skb, route);
}

char _license[] SEC("license") = "GPL";
