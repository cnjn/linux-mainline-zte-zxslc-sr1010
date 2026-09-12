/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ZTE_TAGS_H
#define _ZTE_TAGS_H

#include <linux/crc32.h>
#include <linux/etherdevice.h>
#include <linux/unaligned.h>

#define ZTE_TAGS_HEADER_SIZE 12

/* Return the requested MAC's offset only after validating the whole table. */
static inline int zte_tags_mac_offset(const u8 *table, size_t size, u16 tag)
{
	size_t pos = ZTE_TAGS_HEADER_SIZE;
	u32 len, key_len, value_len;
	int mac_offset = -ENOENT;

	if (size < ZTE_TAGS_HEADER_SIZE ||
	    get_unaligned_le32(table) != 0x33333333)
		return -EINVAL;
	len = get_unaligned_le32(table + 4);
	if (len != size - ZTE_TAGS_HEADER_SIZE)
		return -EINVAL;
	if ((crc32_le(~0U, table + ZTE_TAGS_HEADER_SIZE, len) ^ ~0U) !=
	    get_unaligned_le32(table + 8))
		return -EBADMSG;

	while (pos < size) {
		if (size - pos < 16)
			return -EINVAL;
		key_len = get_unaligned_le32(table + pos);
		value_len = get_unaligned_le32(table + pos + 4);
		pos += 16;
		if (!key_len || key_len > size - pos)
			return -EINVAL;
		if (value_len > size - pos - key_len)
			return -EINVAL;
		if (key_len == 2 && get_unaligned_le16(table + pos) == tag) {
			if (mac_offset >= 0 || value_len != ETH_ALEN ||
			    !is_valid_ether_addr(table + pos + key_len))
				return -EINVAL;
			mac_offset = pos + key_len;
		}
		pos += key_len + value_len;
	}

	return mac_offset;
}

#endif
