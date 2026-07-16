// SPDX-License-Identifier: GPL-2.0-only
/*
 * ZTE ZX279133 eFuse NVMEM provider
 */

#include <linux/align.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/unaligned.h>

#define ZX279133_EFUSE_SHADOW_SIZE	0x80

enum zx279133_efuse_clk {
	ZX279133_EFUSE_CLK_PCLK,
	ZX279133_EFUSE_CLK_WCLK,
	ZX279133_EFUSE_NUM_CLKS,
};

struct zx279133_efuse {
	void __iomem *base;
	struct clk_bulk_data clks[ZX279133_EFUSE_NUM_CLKS];
};

static int zx279133_efuse_read(void *context, unsigned int offset,
			       void *val, size_t bytes)
{
	struct zx279133_efuse *efuse = context;
	u8 *buf = val;
	size_t pos;
	int ret;

	if (!IS_ALIGNED(offset, sizeof(u32)) ||
	    !IS_ALIGNED(bytes, sizeof(u32)))
		return -EINVAL;

	if (offset > ZX279133_EFUSE_SHADOW_SIZE ||
	    bytes > ZX279133_EFUSE_SHADOW_SIZE - offset)
		return -EINVAL;

	ret = clk_bulk_prepare_enable(ZX279133_EFUSE_NUM_CLKS, efuse->clks);
	if (ret)
		return ret;

	for (pos = 0; pos < bytes; pos += sizeof(u32))
		put_unaligned_le32(readl(efuse->base + offset + pos), buf + pos);

	clk_bulk_disable_unprepare(ZX279133_EFUSE_NUM_CLKS, efuse->clks);

	return 0;
}

static int zx279133_efuse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvmem_config config = {
		.dev = dev,
		.name = "zx279133-efuse",
		.id = NVMEM_DEVID_NONE,
		.owner = THIS_MODULE,
		.type = NVMEM_TYPE_OTP,
		.read_only = true,
		.root_only = true,
		.ignore_wp = true,
		.reg_read = zx279133_efuse_read,
		.size = ZX279133_EFUSE_SHADOW_SIZE,
		.word_size = sizeof(u32),
		.stride = sizeof(u32),
	};
	struct zx279133_efuse *efuse;
	struct nvmem_device *nvmem;
	struct resource *res;
	int ret;

	efuse = devm_kzalloc(dev, sizeof(*efuse), GFP_KERNEL);
	if (!efuse)
		return -ENOMEM;

	efuse->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(efuse->base))
		return dev_err_probe(dev, PTR_ERR(efuse->base),
				     "failed to map registers\n");

	if (resource_size(res) < ZX279133_EFUSE_SHADOW_SIZE)
		return dev_err_probe(dev, -EINVAL,
				     "register resource is smaller than %#x bytes\n",
				     ZX279133_EFUSE_SHADOW_SIZE);

	efuse->clks[ZX279133_EFUSE_CLK_PCLK].id = "pclk";
	efuse->clks[ZX279133_EFUSE_CLK_WCLK].id = "wclk";
	ret = devm_clk_bulk_get(dev, ZX279133_EFUSE_NUM_CLKS, efuse->clks);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get clocks\n");

	config.priv = efuse;
	nvmem = devm_nvmem_register(dev, &config);
	if (IS_ERR(nvmem))
		return dev_err_probe(dev, PTR_ERR(nvmem),
				     "failed to register NVMEM provider\n");

	return 0;
}

static const struct of_device_id zx279133_efuse_of_match[] = {
	{ .compatible = "zte,zx279133-efuse" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_efuse_of_match);

static struct platform_driver zx279133_efuse_driver = {
	.probe = zx279133_efuse_probe,
	.driver = {
		.name = "zx279133-efuse",
		.of_match_table = zx279133_efuse_of_match,
	},
};
module_platform_driver(zx279133_efuse_driver);

MODULE_DESCRIPTION("ZTE ZX279133 eFuse NVMEM provider");
MODULE_LICENSE("GPL");
