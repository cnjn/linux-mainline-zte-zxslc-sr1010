// SPDX-License-Identifier: GPL-2.0-only
/* SR1010 U-Boot passes a little-endian structure, not DT cells. */
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/soc/zte/zx279133-bootinfo.h>
#include <linux/sysfs.h>
#include <linux/unaligned.h>

#define ZX279133_BOOTINFO_SIZE	1400
#define ZX279133_SLOT_COUNT	0x38
#define ZX279133_BOOT_SLOT	0x3c
#define ZX279133_ROOTFS_SLOT	0x134
#define ZX279133_SLOT_RECORD	0x13c
#define ZX279133_SLOT_STRIDE	0x21c
#define ZX279133_FW_HEADER	0x1c

static int zx279133_bootinfo_decode(const void *data, size_t len,
				    struct zx279133_bootinfo *info)
{
	struct zx279133_bootinfo decoded;
	const u8 *blob = data, *record, *header;
	u32 count;

	if (len != ZX279133_BOOTINFO_SIZE)
		return -EINVAL;

	if (get_unaligned_le32(blob) != 0xcccccccc ||
	    get_unaligned_le32(blob + 4) != 0x55555555 ||
	    get_unaligned_le32(blob + 8) != 0xaaaaaaaa ||
	    get_unaligned_le32(blob + 12) != 0x11111111)
		return -EINVAL;

	count = get_unaligned_le32(blob + ZX279133_SLOT_COUNT);
	/* cspboot initializes this structure before any firmware is selected. */
	if (!count)
		return -ENODATA;
	decoded.slot = get_unaligned_le32(blob + ZX279133_BOOT_SLOT);
	if (count > 2 || decoded.slot >= count ||
	    decoded.slot != get_unaligned_le32(blob + ZX279133_ROOTFS_SLOT))
		return -EINVAL;

	record = blob + ZX279133_SLOT_RECORD +
		 decoded.slot * ZX279133_SLOT_STRIDE;
	header = record + ZX279133_FW_HEADER;
	/* The bootloader marks a slot invalid after a failed load. */
	if (get_unaligned_le32(record) ||
	    get_unaligned_le32(header + 0xf4) != 0x33333333 ||
	    get_unaligned_le32(header + 0xf8) != 0x66666666 ||
	    get_unaligned_le32(header + 0xfc) != 0x99999999 ||
	    get_unaligned_le32(header + 0x100) != 0xcccccccc)
		return -EINVAL;

	decoded.header_offset = get_unaligned_le32(record + 8);
	decoded.slot_offset = get_unaligned_le32(record + 0x10);
	decoded.kernel_offset = get_unaligned_le32(record + 0x14);
	decoded.rootfs_offset = get_unaligned_le32(record + 0x18);
	/* This is the declared payload size, not the physical span to the header. */
	decoded.rootfs_size = get_unaligned_le32(header + 0x40);
	if (decoded.slot_offset >= decoded.kernel_offset ||
	    decoded.kernel_offset >= decoded.header_offset)
		return -EINVAL;

	/* A RAM-root image may have no flash rootfs. */
	if (decoded.rootfs_size &&
	    (decoded.rootfs_offset <= decoded.kernel_offset ||
	     decoded.rootfs_offset >= decoded.header_offset ||
	     decoded.rootfs_size > decoded.header_offset - decoded.rootfs_offset))
		return -EINVAL;

	*info = decoded;
	return 0;
}

/**
 * zx279133_get_bootinfo - decode the vendor's selected SR1010 firmware slot
 * @info: decoded information, unchanged on error
 *
 * Return: 0 on success, -ENOENT when versioninfo is absent, -ENODATA before
 * firmware selection, -EINVAL for an unsupported or malformed record, or
 * -ENODEV on other machines. Call after the device tree has been unflattened.
 * No flash is read or modified. The record describes bootloader state, not
 * proof that the currently running kernel was loaded from flash.
 */
int zx279133_get_bootinfo(struct zx279133_bootinfo *info)
{
	const void *data;
	int len;

	if (!of_machine_is_compatible("zte,zx279133-sr1010"))
		return -ENODEV;

	data = of_get_property(of_chosen, "versioninfo", &len);
	if (!data)
		return -ENOENT;

	return zx279133_bootinfo_decode(data, len, info);
}
EXPORT_SYMBOL_GPL(zx279133_get_bootinfo);

static struct zx279133_bootinfo bootinfo;

#define ZX279133_BOOTINFO_ATTR(_name, _format) \
static ssize_t _name##_show(struct kobject *kobj, \
			   struct kobj_attribute *attr, char *buf) \
{ \
	return sysfs_emit(buf, _format "\n", bootinfo._name); \
} \
static struct kobj_attribute _name##_attr = __ATTR_RO(_name)

ZX279133_BOOTINFO_ATTR(slot, "%u");
ZX279133_BOOTINFO_ATTR(slot_offset, "0x%08x");
ZX279133_BOOTINFO_ATTR(kernel_offset, "0x%08x");
ZX279133_BOOTINFO_ATTR(rootfs_offset, "0x%08x");
ZX279133_BOOTINFO_ATTR(rootfs_size, "0x%08x");
ZX279133_BOOTINFO_ATTR(header_offset, "0x%08x");

static struct attribute *zx279133_bootinfo_attrs[] = {
	&slot_attr.attr,
	&slot_offset_attr.attr,
	&kernel_offset_attr.attr,
	&rootfs_offset_attr.attr,
	&rootfs_size_attr.attr,
	&header_offset_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(zx279133_bootinfo);

static int __init zx279133_bootinfo_init(void)
{
	struct kobject *kobj;
	int ret;

	ret = zx279133_get_bootinfo(&bootinfo);
	if (ret == -ENODEV || ret == -ENOENT)
		return 0;
	if (ret == -ENODATA) {
		pr_info("zx279133: bootloader has not selected a flash firmware\n");
		return 0;
	}
	if (ret) {
		pr_warn("zx279133: ignoring invalid /chosen/versioninfo\n");
		return ret;
	}

	pr_info("zx279133: firmware slot %u at %#x, kernel %#x, rootfs %#x+%#x, header %#x\n",
		bootinfo.slot, bootinfo.slot_offset, bootinfo.kernel_offset,
		bootinfo.rootfs_offset, bootinfo.rootfs_size, bootinfo.header_offset);
	kobj = kobject_create_and_add("zx279133", firmware_kobj);
	if (!kobj)
		return -ENOMEM;

	ret = sysfs_create_groups(kobj, zx279133_bootinfo_groups);
	if (ret)
		kobject_put(kobj);
	return ret;
}
subsys_initcall(zx279133_bootinfo_init);
