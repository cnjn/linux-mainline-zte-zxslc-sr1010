/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LINUX_SOC_ZTE_ZX279133_BOOTINFO_H
#define __LINUX_SOC_ZTE_ZX279133_BOOTINFO_H

#include <linux/errno.h>
#include <linux/kconfig.h>
#include <linux/types.h>

/* Byte offsets in the whole flash; slot is zero-based. */
struct zx279133_bootinfo {
	u32 slot;
	u32 slot_offset;
	u32 kernel_offset;
	u32 rootfs_offset;
	u32 rootfs_size;
	u32 header_offset;
};

#if IS_ENABLED(CONFIG_ZTE_ZX279133_BOOTINFO)
int zx279133_get_bootinfo(struct zx279133_bootinfo *info);
#else
static inline int zx279133_get_bootinfo(struct zx279133_bootinfo *info)
{
	return -ENODEV;
}
#endif

#endif
