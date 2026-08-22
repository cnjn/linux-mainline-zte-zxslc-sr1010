// SPDX-License-Identifier: GPL-2.0-only
/*
 * ZTE ZX279133 internal Uni SerDes PHY.
 *
 * The SR1010 LAN CPU uplink uses XMAC0 work mode 5: Uni SerDes mode 1
 * (the vendor 10G Ethernet profile) followed by XPCS0 USXGMII mode 0.
 * This provider owns only the internal Uni SerDes block; XMAC0/XPCS0 and
 * the RTL8372N switch are separate consumers.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#define ZX279133_UNI_SERDES_PROFILE_WORDS	49
#define ZX279133_UNI_SERDES_MODE_CTRL		0x54
#define ZX279133_UNI_SERDES_ENABLE		BIT(0)
#define ZX279133_UNI_SERDES_PLL_STATUS		0xd0
#define ZX279133_UNI_SERDES_PLL_LOCK		BIT(0)
#define ZX279133_UNI_SERDES_RX_STATUS		0xe4
#define ZX279133_UNI_SERDES_RX_LOS		BIT(0)
#define ZX279133_UNI_SERDES_CDR_LOCK		BIT(1)
#define ZX279133_UNI_SERDES_POLL_US		2000
#define ZX279133_UNI_SERDES_TIMEOUT_US		2000000
#define ZX279133_UNI_RESET_PHASE_US		4300

struct zx279133_uni_serdes {
	struct device *dev;
	void __iomem *serdes;
	struct clk_bulk_data clocks[2];
	struct reset_control_bulk_data resets[2];
	struct phy *phy;
	/* Protects the powered state and clock/reset lifecycle. */
	struct mutex lock;
	bool powered;
};

/* Vendor uni_mode_eth_10gbase_r_cfg(), used for Uni mode 1. */
static const u32 zx279133_uni_10g_profile[] = {
	0xe0000004, 0x50a840a3, 0x013e8687, 0x0210c073,
	0x00000048, 0x00000000, 0x00ff8000, 0x00238020,
	0x80000400, 0x00020000, 0x0c633830, 0x00000e46,
	0x00000020, 0x00002000, 0x00000000, 0x00ff0000,
	0x05a85100, 0x00000000, 0x4f042b2a, 0x60002200,
	0x34000003, 0x00000100, 0x00000080, 0x00000000,
	0x200554a8, 0x67748091, 0x01000000, 0x40003b00,
	0xa9004000, 0x10670002, 0x000000da, 0xf20000c8,
	0x10371037, 0x01000200, 0x00020001, 0x10000000,
	0x00084008, 0x802dc000, 0x000000ff, 0x55555500,
	0x55555555, 0x00555555, 0x30000818, 0x40002000,
	0x0000000c, 0x01000000, 0x00000080, 0x00010000,
	0x00000000,
};

static_assert(ARRAY_SIZE(zx279133_uni_10g_profile) ==
	      ZX279133_UNI_SERDES_PROFILE_WORDS);

static int zx279133_uni_serdes_reset_pulse(struct zx279133_uni_serdes *priv)
{
	int ret;

	ret = reset_control_bulk_assert(ARRAY_SIZE(priv->resets), priv->resets);
	if (ret)
		return ret;
	usleep_range(ZX279133_UNI_RESET_PHASE_US,
		     ZX279133_UNI_RESET_PHASE_US + 500);

	ret = reset_control_bulk_deassert(ARRAY_SIZE(priv->resets), priv->resets);
	if (ret)
		return ret;
	usleep_range(ZX279133_UNI_RESET_PHASE_US,
		     ZX279133_UNI_RESET_PHASE_US + 500);

	return 0;
}

static void zx279133_uni_serdes_write_profile(struct zx279133_uni_serdes *priv)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_uni_10g_profile); i++)
		writel(zx279133_uni_10g_profile[i], priv->serdes + i * 4);
}

static int zx279133_uni_serdes_set_mode(struct phy *phy, enum phy_mode mode,
					int submode)
{
	if (mode != PHY_MODE_ETHERNET)
		return -EOPNOTSUPP;

	if (submode != PHY_INTERFACE_MODE_USXGMII &&
	    submode != PHY_INTERFACE_MODE_10GBASER)
		return -EOPNOTSUPP;

	return 0;
}

static int zx279133_uni_serdes_power_on(struct phy *phy)
{
	struct zx279133_uni_serdes *priv = phy_get_drvdata(phy);
	u32 status;
	int ret;

	mutex_lock(&priv->lock);
	if (priv->powered) {
		ret = 0;
		goto out_unlock;
	}

	ret = clk_bulk_prepare_enable(ARRAY_SIZE(priv->clocks), priv->clocks);
	if (ret)
		goto out_unlock;

	ret = zx279133_uni_serdes_reset_pulse(priv);
	if (ret)
		goto err_disable_clocks;

	zx279133_uni_serdes_write_profile(priv);
	status = readl(priv->serdes + ZX279133_UNI_SERDES_MODE_CTRL);
	writel(status | ZX279133_UNI_SERDES_ENABLE,
	       priv->serdes + ZX279133_UNI_SERDES_MODE_CTRL);

	ret = readl_poll_timeout(priv->serdes + ZX279133_UNI_SERDES_PLL_STATUS,
				 status, status & ZX279133_UNI_SERDES_PLL_LOCK,
				 ZX279133_UNI_SERDES_POLL_US,
				 ZX279133_UNI_SERDES_TIMEOUT_US);
	if (ret)
		goto err_disable_clocks;

	status = readl(priv->serdes + ZX279133_UNI_SERDES_RX_STATUS);
	if (!(status & ZX279133_UNI_SERDES_RX_LOS)) {
		ret = readl_poll_timeout(priv->serdes + ZX279133_UNI_SERDES_RX_STATUS,
					 status,
					 status & ZX279133_UNI_SERDES_CDR_LOCK,
					 ZX279133_UNI_SERDES_POLL_US,
					 ZX279133_UNI_SERDES_TIMEOUT_US);
		if (ret)
			goto err_disable_clocks;
	}

	dev_info(priv->dev, "Uni SerDes 10G profile ready, status %#x/%#x\n",
		 readl(priv->serdes + ZX279133_UNI_SERDES_PLL_STATUS),
		 readl(priv->serdes + ZX279133_UNI_SERDES_RX_STATUS));
	priv->powered = true;
	ret = 0;
	goto out_unlock;

err_disable_clocks:
	writel(readl(priv->serdes + ZX279133_UNI_SERDES_MODE_CTRL) &
	       ~ZX279133_UNI_SERDES_ENABLE,
	       priv->serdes + ZX279133_UNI_SERDES_MODE_CTRL);
	clk_bulk_disable_unprepare(ARRAY_SIZE(priv->clocks), priv->clocks);
out_unlock:
	mutex_unlock(&priv->lock);
	return ret;
}

static int zx279133_uni_serdes_power_off(struct phy *phy)
{
	struct zx279133_uni_serdes *priv = phy_get_drvdata(phy);

	mutex_lock(&priv->lock);
	if (priv->powered) {
		writel(readl(priv->serdes + ZX279133_UNI_SERDES_MODE_CTRL) &
		       ~ZX279133_UNI_SERDES_ENABLE,
		       priv->serdes + ZX279133_UNI_SERDES_MODE_CTRL);
		clk_bulk_disable_unprepare(ARRAY_SIZE(priv->clocks), priv->clocks);
		priv->powered = false;
	}
	mutex_unlock(&priv->lock);

	return 0;
}

static const struct phy_ops zx279133_uni_serdes_ops = {
	.set_mode = zx279133_uni_serdes_set_mode,
	.power_on = zx279133_uni_serdes_power_on,
	.power_off = zx279133_uni_serdes_power_off,
	.owner = THIS_MODULE,
};

static void zx279133_uni_serdes_cleanup(void *data)
{
	struct zx279133_uni_serdes *priv = data;

	if (priv->phy)
		zx279133_uni_serdes_power_off(priv->phy);
}

static int zx279133_uni_serdes_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_uni_serdes *priv;
	struct phy_provider *provider;
	struct phy *phy;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	mutex_init(&priv->lock);
	priv->serdes = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->serdes))
		return PTR_ERR(priv->serdes);

	priv->clocks[0].id = "pclk";
	priv->clocks[1].id = "ref50m";
	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(priv->clocks), priv->clocks);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get Uni SerDes clocks\n");

	priv->resets[0].id = "rx";
	priv->resets[1].id = "tx";
	ret = devm_reset_control_bulk_get_exclusive(dev,
						    ARRAY_SIZE(priv->resets),
						    priv->resets);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get Uni SerDes resets\n");

	phy = devm_phy_create(dev, NULL, &zx279133_uni_serdes_ops);
	if (IS_ERR(phy))
		return dev_err_probe(dev, PTR_ERR(phy), "failed to create PHY\n");

	phy_set_drvdata(phy, priv);
	priv->phy = phy;
	platform_set_drvdata(pdev, priv);

	ret = devm_add_action_or_reset(dev, zx279133_uni_serdes_cleanup, priv);
	if (ret)
		return ret;

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	return PTR_ERR_OR_ZERO(provider);
}

static const struct of_device_id zx279133_uni_serdes_of_match[] = {
	{ .compatible = "zte,zx279133-uni-serdes" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_uni_serdes_of_match);

static struct platform_driver zx279133_uni_serdes_driver = {
	.probe = zx279133_uni_serdes_probe,
	.driver = {
		.name = "zx279133-uni-serdes",
		.of_match_table = zx279133_uni_serdes_of_match,
	},
};
module_platform_driver(zx279133_uni_serdes_driver);

MODULE_DESCRIPTION("ZTE ZX279133 internal Uni SerDes PHY");
MODULE_LICENSE("GPL");
