// SPDX-License-Identifier: GPL-2.0-only
/* Factory-style RTL8372N S-VLAN tag on the SR1010 external CPU port. */
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <net/dsa_zx279133_rtl8372n.h>

#include "tag.h"

#define ZX279133_RTL8372N_TAG_NAME	"zx279133-rtl8372n"

static struct sk_buff *
zx279133_rtl8372n_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct dsa_port *dp = dsa_user_to_port(netdev);
	__be16 tag[VLAN_HLEN / sizeof(__be16)] = {
		htons(ETH_P_8021Q),
		htons(ZX279133_RTL8372N_TRANSPORT_VID_BASE + dp->index),
	};

	/* The switch removes the service tag before transmission, so pad the
	 * original Ethernet frame rather than relying on conduit-side padding.
	 */
	if (unlikely(eth_skb_pad(skb)))
		return NULL;

	skb_push(skb, VLAN_HLEN);
	dsa_alloc_etype_header(skb, VLAN_HLEN);
	memcpy(dsa_etype_header_pos_tx(skb), tag, sizeof(tag));

	return skb;
}

static struct sk_buff *
zx279133_rtl8372n_rcv(struct sk_buff *skb, struct net_device *netdev)
{
	__be16 tag[VLAN_HLEN / sizeof(__be16)];
	bool inline_tag = false;
	__be16 vlan_proto;
	u16 tci;
	u16 vid;
	u8 port;

	if (skb_vlan_tag_present(skb)) {
		vlan_proto = skb->vlan_proto;
		tci = skb_vlan_tag_get(skb);
	} else {
		if (unlikely(!pskb_may_pull(skb, VLAN_HLEN)))
			return NULL;
		memcpy(tag, dsa_etype_header_pos_rx(skb), sizeof(tag));
		vlan_proto = tag[0];
		tci = ntohs(tag[1]);
		inline_tag = true;
	}
	if (unlikely(!eth_type_vlan(vlan_proto)))
		return NULL;

	vid = tci & VLAN_VID_MASK;
	if (unlikely(vid < ZX279133_RTL8372N_TRANSPORT_VID_BASE +
			   ZX279133_RTL8372N_USER_PORT_MIN ||
		     vid > ZX279133_RTL8372N_TRANSPORT_VID_BASE +
			   ZX279133_RTL8372N_USER_PORT_MAX))
		return NULL;
	port = vid - ZX279133_RTL8372N_TRANSPORT_VID_BASE;

	skb->dev = dsa_conduit_find_user(netdev, 0, port);
	if (!skb->dev)
		return NULL;

	if (inline_tag) {
		skb_pull_rcsum(skb, VLAN_HLEN);
		dsa_strip_etype_header(skb, VLAN_HLEN);
	} else {
		__vlan_hwaccel_clear_tag(skb);
	}
	dsa_default_offload_fwd_mark(skb);

	return skb;
}

static const struct dsa_device_ops zx279133_rtl8372n_netdev_ops = {
	.name			= ZX279133_RTL8372N_TAG_NAME,
	.proto			= DSA_TAG_PROTO_ZX279133_RTL8372N,
	.xmit			= zx279133_rtl8372n_xmit,
	.rcv			= zx279133_rtl8372n_rcv,
	.needed_headroom	= VLAN_HLEN,
	.promisc_on_conduit	= true,
};

MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_ZX279133_RTL8372N,
			    ZX279133_RTL8372N_TAG_NAME);
module_dsa_tag_driver(zx279133_rtl8372n_netdev_ops);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DSA S-VLAN tagger for ZX279133 connected RTL8372N");
