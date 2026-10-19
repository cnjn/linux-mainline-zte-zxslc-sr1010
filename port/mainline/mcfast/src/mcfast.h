/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * mcfast: shared definitions between the tc BPF program and the daemon.
 *
 * One map entry per resolved kernel multicast route (MFC entry), one map per
 * address family: the key is the ingress interface plus (source, group),
 * source all-zero for (*,G) routes.
 */
#ifndef MCFAST_H
#define MCFAST_H

#define MCFAST_MAX_ROUTES	1024
#define MCFAST_MAX_OIFS		4

struct mcfast_key {
	__u32 ifindex;
	__be32 src;
	__be32 grp;
};

struct mcfast_key6 {
	__u32 ifindex;
	__u8 src[16];
	__u8 grp[16];
};

struct mcfast_oif {
	__u32 ifindex;
	__u8 mac[6];
	__u16 pad;
};

struct mcfast_route {
	__u8 ttl_min;		/* forward only when the TTL / hop limit is above */
	__u8 n_oifs;
	__u16 mtu;		/* smallest egress MTU */
	__u32 pad;
	struct mcfast_oif oif[MCFAST_MAX_OIFS];
	/* Updated by the BPF program; the daemon carries them over. */
	__u64 packets;
	__u64 bytes;
};

#endif
