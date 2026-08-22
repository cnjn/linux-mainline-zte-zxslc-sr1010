/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _NET_DSA_ZX279133_RTL8372N_H
#define _NET_DSA_ZX279133_RTL8372N_H

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <net/dsa.h>

#define ZX279133_RTL8372N_USER_PORT_MIN	4
#define ZX279133_RTL8372N_USER_PORT_MAX	7
#define ZX279133_RTL8372N_ACCESS_UNTAGGED	BIT(16)

struct zx279133_rtl8372n_tagger_data {
	u32 access_vlan[ZX279133_RTL8372N_USER_PORT_MAX + 1];
};

static inline int zx279133_tagger_set_access_vlan(struct dsa_switch *ds,
						  int port, u16 pvid,
						  bool untagged)
{
	struct zx279133_rtl8372n_tagger_data *data;

	if (port < ZX279133_RTL8372N_USER_PORT_MIN ||
	    port > ZX279133_RTL8372N_USER_PORT_MAX)
		return -EINVAL;

	data = READ_ONCE(ds->tagger_data);
	if (!data)
		return -ENODEV;

	WRITE_ONCE(data->access_vlan[port],
		   pvid | (untagged ? ZX279133_RTL8372N_ACCESS_UNTAGGED : 0));
	return 0;
}

static inline u32
zx279133_tagger_get_access_vlan(struct dsa_switch *ds, int port)
{
	struct zx279133_rtl8372n_tagger_data *data;

	if (port < ZX279133_RTL8372N_USER_PORT_MIN ||
	    port > ZX279133_RTL8372N_USER_PORT_MAX)
		return 0;

	data = READ_ONCE(ds->tagger_data);
	if (!data)
		return 0;

	return READ_ONCE(data->access_vlan[port]);
}

#endif /* _NET_DSA_ZX279133_RTL8372N_H */
