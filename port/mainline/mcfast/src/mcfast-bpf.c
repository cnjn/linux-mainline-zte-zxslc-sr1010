// SPDX-License-Identifier: GPL-2.0-only
/*
 * mcfast: forward routed IPv4 and IPv6 multicast at tc ingress.
 *
 * For packets matching a resolved kernel multicast route the program does
 * what ip_mr_forward() / ip6_mr_forward() would (TTL or hop limit check and
 * decrement, Ethernet header for the egress) and redirects to each egress
 * device, typically a bridge, which still applies its own snooping state.
 * Everything else continues to the next program and the regular stack.
 */
#define KBUILD_MODNAME "mcfast"
#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/ipv6.h>
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

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MCFAST_MAX_ROUTES);
	__type(key, struct mcfast_key6);
	__type(value, struct mcfast_route);
} mcfast_routes6 SEC(".maps");

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

static __always_inline struct mcfast_route *
mcfast_lookup6(__u32 ifindex, struct ipv6hdr *ip6)
{
	struct mcfast_key6 key = {
		.ifindex = ifindex,
	};
	struct mcfast_route *route;
	__u8 *grp = ip6->daddr.in6_u.u6_addr8;

	/* Interface- and link-local scoped groups are never routed. */
	if (grp[0] != 0xff || (grp[1] & 0x0f) <= 2)
		return NULL;

	__builtin_memcpy(key.src, ip6->saddr.in6_u.u6_addr8, 16);
	__builtin_memcpy(key.grp, grp, 16);
	route = bpf_map_lookup_elem(&mcfast_routes6, &key);
	if (route)
		return route;
	__builtin_memset(key.src, 0, 16);
	return bpf_map_lookup_elem(&mcfast_routes6, &key);
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

/*
 * Give the packet whose network header is at @l3 (0 or ETH_HLEN) an
 * Ethernet header with the group address @dmac; the source address is
 * written per egress device by mcfast_xmit().
 */
static __always_inline int
mcfast_l2(struct __sk_buff *skb, __u32 l3, const __u8 *dmac, __u16 proto)
{
	struct ethhdr eth = {
		.h_proto = bpf_htons(proto),
	};

	__builtin_memcpy(eth.h_dest, dmac, ETH_ALEN);
	if (!l3 && bpf_skb_change_head(skb, ETH_HLEN, 0))
		return -1;
	return bpf_skb_store_bytes(skb, 0, &eth, sizeof(eth), 0);
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

static __always_inline int
mcfast_fwd4(struct __sk_buff *skb, __u32 l3)
{
	void *data = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;
	struct iphdr *ip = data + l3;
	struct mcfast_route *route;
	__u8 mac[ETH_ALEN];
	__u32 g;

	if ((void *)(ip + 1) > data_end || ip->version != 4)
		return TC_ACT_UNSPEC;
	route = mcfast_lookup(skb->ifindex, ip);
	if (!route || ip->ttl <= route->ttl_min || skb->len - l3 > route->mtu)
		return TC_ACT_UNSPEC;

	g = bpf_ntohl(ip->daddr);
	mac[0] = 0x01;
	mac[1] = 0x00;
	mac[2] = 0x5e;
	mac[3] = (g >> 16) & 0x7f;
	mac[4] = g >> 8;
	mac[5] = g;
	if (mcfast_ttl_dec(skb, ip, l3) || mcfast_l2(skb, l3, mac, ETH_P_IP))
		return TC_ACT_SHOT;
	return mcfast_xmit(skb, route);
}

static __always_inline int
mcfast_fwd6(struct __sk_buff *skb, __u32 l3)
{
	void *data = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;
	struct ipv6hdr *ip6 = data + l3;
	struct mcfast_route *route;
	__u8 mac[ETH_ALEN];

	if ((void *)(ip6 + 1) > data_end || ip6->version != 6)
		return TC_ACT_UNSPEC;
	route = mcfast_lookup6(skb->ifindex, ip6);
	if (!route || ip6->hop_limit <= route->ttl_min ||
	    skb->len - l3 > route->mtu)
		return TC_ACT_UNSPEC;

	mac[0] = 0x33;
	mac[1] = 0x33;
	__builtin_memcpy(&mac[2], &ip6->daddr.in6_u.u6_addr8[12], 4);
	/* No header checksum in IPv6. */
	ip6->hop_limit--;
	if (mcfast_l2(skb, l3, mac, ETH_P_IPV6))
		return TC_ACT_SHOT;
	return mcfast_xmit(skb, route);
}

/* Ethernet ingress, e.g. an IPoE WAN. */
SEC("tcx/ingress")
int mcfast_eth(struct __sk_buff *skb)
{
	void *data = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;
	struct ethhdr *eth = data;

	if ((void *)(eth + 1) > data_end)
		return TC_ACT_UNSPEC;
	if (eth->h_proto == bpf_htons(ETH_P_IP))
		return mcfast_fwd4(skb, ETH_HLEN);
	if (eth->h_proto == bpf_htons(ETH_P_IPV6))
		return mcfast_fwd6(skb, ETH_HLEN);
	return TC_ACT_UNSPEC;
}

/* Ingress without a link-layer header, e.g. a PPPoE session device. */
SEC("tcx/ingress")
int mcfast_ip(struct __sk_buff *skb)
{
	if (skb->protocol == bpf_htons(ETH_P_IP))
		return mcfast_fwd4(skb, 0);
	if (skb->protocol == bpf_htons(ETH_P_IPV6))
		return mcfast_fwd6(skb, 0);
	return TC_ACT_UNSPEC;
}

char _license[] SEC("license") = "GPL";
