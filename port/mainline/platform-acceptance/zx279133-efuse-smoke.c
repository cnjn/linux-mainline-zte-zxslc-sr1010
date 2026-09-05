// SPDX-License-Identifier: GPL-2.0-only
#include <linux/crc32.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#define ZX279133_EFUSE_BYTES 0x80

static int __init zx279133_efuse_smoke_init(void)
{
	struct nvmem_device *nvmem;
	struct device *efuse_dev;
	u8 first[ZX279133_EFUSE_BYTES];
	u8 second[ZX279133_EFUSE_BYTES];
	u32 crc;
	int ret;

	efuse_dev = bus_find_device_by_name(&platform_bus_type, NULL,
					     "14f11000.efuse");
	if (!efuse_dev)
		return -ENODEV;

	nvmem = nvmem_device_get(efuse_dev, "zx279133-efuse");
	put_device(efuse_dev);
	if (IS_ERR(nvmem))
		return PTR_ERR(nvmem);

	ret = nvmem_device_read(nvmem, 0, sizeof(first), first);
	if (ret != sizeof(first)) {
		if (ret >= 0)
			ret = -EIO;
		goto out_put;
	}
	ret = nvmem_device_read(nvmem, 0, sizeof(second), second);
	if (ret != sizeof(second)) {
		if (ret >= 0)
			ret = -EIO;
		goto out_put;
	}
	if (memcmp(first, second, sizeof(first))) {
		ret = -EIO;
		goto out_put;
	}

	crc = crc32_le(~0U, first, sizeof(first)) ^ ~0U;
	pr_info("zx279133-efuse-smoke: bytes=%zu crc32=%08x repeat=match PASS\n",
		sizeof(first), crc);
	ret = 0;

out_put:
	nvmem_device_put(nvmem);
	return ret;
}

static void __exit zx279133_efuse_smoke_exit(void)
{
	pr_info("zx279133-efuse-smoke: unloaded\n");
}

module_init(zx279133_efuse_smoke_init);
module_exit(zx279133_efuse_smoke_exit);

MODULE_DESCRIPTION("Read-only ZX279133 eFuse acceptance smoke test");
MODULE_LICENSE("GPL");
