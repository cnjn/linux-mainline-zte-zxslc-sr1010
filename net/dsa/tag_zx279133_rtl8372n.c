// SPDX-License-Identifier: GPL-2.0-only
/*
 * Fixed CPU-link tag for the SR1010 ZX279133-to-RTL8372N connection.
 *
 * The SR1010 LAN mapping assigns private transport VLANs 59..62 to physical
 * ports 4..7. Untagged access VLANs are carried as software metadata inside
 * that private transport contract; the private VLAN IDs are never exposed as
 * customer VLAN state.
 */
#include <linux/dsa/8021q.h>
#include <linux/if_vlan.h>
#include <linux/slab.h>
#include <net/dsa_zx279133_rtl8372n.h>

#include "tag.h"
#include "tag_8021q.h"

#define ZX279133_RTL8372N_TAG_NAME	"zx279133-rtl8372n"
static int zx279133_rtl8372n_connect(struct dsa_switch *ds)
{
	struct zx279133_rtl8372n_tagger_data *data;

	if (ds->tagger_data)
		return -EBUSY;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ds->tagger_data = data;
	return 0;
}

static void zx279133_rtl8372n_disconnect(struct dsa_switch *ds)
{
	kfree(ds->tagger_data);
	ds->tagger_data = NULL;
}

static bool zx279133_rtl8372n_transport_vid(u16 vid)
{
	return vid >= ZX279133_RTL8372N_TRANSPORT_VID_BASE +
		      ZX279133_RTL8372N_USER_PORT_MIN &&
	       vid <= ZX279133_RTL8372N_TRANSPORT_VID_BASE +
		      ZX279133_RTL8372N_USER_PORT_MAX;
}

static struct sk_buff *
zx279133_rtl8372n_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct dsa_port *dp = dsa_user_to_port(netdev);
	u32 access;
	u16 vid;

	if (!dp || !dp->ds->tagger_data)
		return NULL;
	if (dp->index < ZX279133_RTL8372N_USER_PORT_MIN ||
	    dp->index > ZX279133_RTL8372N_USER_PORT_MAX)
		return NULL;

	vid = ZX279133_RTL8372N_TRANSPORT_VID_BASE + dp->index;
	if (dp->index == ZX279133_RTL8372N_USER_PORT_MAX)
		vid = ZX279133_RTL8372N_LAN1_TX_VID;
	access = zx279133_tagger_get_access_vlan(dp->ds, dp->index);
	if (access & ZX279133_RTL8372N_ACCESS_UNTAGGED) {
		u16 access_pvid = access & VLAN_VID_MASK;
		u16 skb_vid = 0;
		bool tagged = false;

		if (skb_vlan_tag_present(skb)) {
			tagged = true;
			skb_vid = skb_vlan_tag_get_id(skb);
		} else if (eth_type_vlan(skb->protocol) &&
			   pskb_may_pull(skb, VLAN_ETH_HLEN)) {
			tagged = true;
			skb_vid = ntohs(vlan_eth_hdr(skb)->h_vlan_TCI) &
				  VLAN_VID_MASK;
		}
		if (tagged && skb_vid != access_pvid)
			return NULL;
		if (tagged && skb_vlan_pop(skb))
			return NULL;
		if (skb_vlan_tag_present(skb) || eth_type_vlan(skb->protocol))
			return NULL;
	}
	return dsa_8021q_xmit(skb, netdev, ETH_P_8021Q, vid);
}

static struct sk_buff *
zx279133_rtl8372n_rcv(struct sk_buff *skb, struct net_device *netdev)
{
	struct dsa_port *user_dp;
	struct net_device *user;
	struct dsa_switch *ds;
	u16 access_pvid;
	u16 vid;
	u16 port;

	/* DSA classifies conduit traffic as ETH_P_XDSA before invoking the
	 * tagger, so the private 802.1Q sideband must be recognized from the
	 * first payload bytes rather than skb->protocol.
	 */
	if (!skb_vlan_tag_present(skb) && pskb_may_pull(skb, VLAN_HLEN)) {
		struct vlan_hdr *vhdr = (struct vlan_hdr *)skb->data;

		vid = ntohs(vhdr->h_vlan_TCI) & VLAN_VID_MASK;
		if (zx279133_rtl8372n_transport_vid(vid)) {
			/* LAN-first NPPT initialization preserves the switch's
			 * transport VLAN header in-band. Move it to hwaccel form so
			 * the generic VLAN helper repairs the Ethernet header, then
			 * consume the private CPU-link transport tag below.
			 */
			skb->protocol = htons(ETH_P_8021Q);
			skb = skb_vlan_untag(skb);
			if (!skb)
				return NULL;
		}
	}
	if (!skb_vlan_tag_present(skb))
		return NULL;

	vid = skb_vlan_tag_get_id(skb);
	if (!zx279133_rtl8372n_transport_vid(vid))
		return NULL;
	port = vid - ZX279133_RTL8372N_TRANSPORT_VID_BASE;
	user = dsa_conduit_find_user(netdev, 0, port);
	if (!user)
		return NULL;
	user_dp = dsa_user_to_port(user);
	if (!user_dp || !user_dp->ds || !user_dp->ds->tagger_data)
		return NULL;
	ds = user_dp->ds;
	access_pvid = zx279133_tagger_get_access_vlan(ds, port);
	access_pvid &= VLAN_VID_MASK;

	__vlan_hwaccel_clear_tag(skb);
	if (access_pvid && !skb_vlan_tag_present(skb) &&
	    !eth_type_vlan(skb->protocol))
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), access_pvid);
	skb->dev = user;

	return skb;
}

static const struct dsa_device_ops zx279133_rtl8372n_netdev_ops = {
	.name			= ZX279133_RTL8372N_TAG_NAME,
	.proto			= DSA_TAG_PROTO_ZX279133_RTL8372N,
	.xmit			= zx279133_rtl8372n_xmit,
	.rcv			= zx279133_rtl8372n_rcv,
	.connect		= zx279133_rtl8372n_connect,
	.disconnect		= zx279133_rtl8372n_disconnect,
	.needed_headroom	= VLAN_HLEN,
	.promisc_on_conduit	= true,
};

MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_ZX279133_RTL8372N,
			    ZX279133_RTL8372N_TAG_NAME);
module_dsa_tag_driver(zx279133_rtl8372n_netdev_ops);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DSA tagger for ZX279133 connected RTL8372N");
