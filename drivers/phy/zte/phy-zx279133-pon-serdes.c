// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#define ZX279133_TOPCRM_PON_MUX_CTRL		0x0c
#define ZX279133_TOPCRM_PON_PLL_SEL		0x10
#define ZX279133_TOPCRM_PON_ETH_SEL		BIT(9)
#define ZX279133_TOPCRM_PON_MODE_MASK		GENMASK(9, 8)
#define ZX279133_TOPCRM_PON_PLL_SEL_MASK	GENMASK(5, 4)

#define ZX279133_PON_PLL_CFG0			0x0
#define ZX279133_PON_PLL_CFG1			0x4
#define ZX279133_PON_PLL_CFG0_ETH		0x20106454
#define ZX279133_PON_PLL_CFG0_FW_BIT		BIT(30)
#define ZX279133_PON_PLL_CFG1_LATCH		BIT(28)
#define ZX279133_PON_PLL_CFG1_ETH		0x04000000

#define ZX279133_PON_SERDES_ACTIVATE0		0x40
#define ZX279133_PON_SERDES_ACTIVATE0_EN	BIT(15)
#define ZX279133_PON_SERDES_ACTIVATE1		0x54
#define ZX279133_PON_SERDES_ACTIVATE1_EN	BIT(0)
#define ZX279133_PON_SERDES_MODE_CTRL		0x90
#define ZX279133_PON_SERDES_MODE_MASK		GENMASK(14, 13)
#define ZX279133_PON_SERDES_MODE_EN		BIT(14)
#define ZX279133_PON_SERDES_PLL_STATUS		0xd0
#define ZX279133_PON_SERDES_PLL_LOCK		BIT(0)
#define ZX279133_PON_SERDES_RX_STATUS		0xe4
#define ZX279133_PON_SERDES_RX_LOS		BIT(0)
#define ZX279133_PON_SERDES_CDR_LOCK		BIT(1)

#define ZX279133_PON_SERDES_RESET_US		10000
#define ZX279133_PON_SERDES_PROFILE_WORDS	49
#define ZX279133_PON_SERDES_POLL_US		2000
#define ZX279133_PON_SERDES_LOCK_TIMEOUT_US	2000000

struct zx279133_pon_serdes {
	struct device *dev;
	void __iomem *serdes;
	void __iomem *pll;
	void __iomem *topcrm_mode;
	struct regmap *topcrm;
	struct clk *pclk;
	struct reset_control_bulk_data resets[2];
	/* Protects interface selection and the powered/configured lifecycle. */
	struct mutex lock;
	phy_interface_t interface;
	u32 saved_mux;
	u32 saved_pll_sel;
	u32 saved_pll_cfg0;
	u32 saved_pll_cfg1;
	bool configured;
	bool powered;
};

/* CPU133 mode 9 and mode 15 share this recovered 2.5GBASE-X profile. */
static const u32 zx279133_pon_serdes_2500basex_profile[] = {
	0xe0000004, 0x4fa8c0a2, 0x013e8604, 0x1f51c8f3,
	0x00000044, 0x00194000, 0x0b080000, 0x00238220,
	0x80000100, 0x00050000, 0x0c633830, 0x00000007,
	0x00000020, 0x00002000, 0x00000000, 0x00ff0000,
	0x05a85100, 0x00000000, 0x04005b6a, 0x60002220,
	0x34000003, 0x00000408, 0x00000080, 0x00000000,
	0x200574a8, 0x00649052, 0x01000000, 0x40007700,
	0xa9004000, 0x01108002, 0x000000da, 0xf20000c8,
	0x10371037, 0x01000200, 0x00020001, 0x10000000,
	0x0000001c, 0x802dc000, 0x000000ff, 0x55555500,
	0x55555555, 0x00555555, 0x30000818, 0x40002000,
	0x0000000c, 0x01000000, 0x00000080, 0x00010000,
	0x00000000,
};

/*
 * CPU133 Uni mode 7 (1G SGMII) maps to PON SerDes mode 8 through
 * uni_eth_mode_change(); this is the recovered 49-word mode-8 profile from
 * uni_mode_eth_1gbase_x_cfg().
 */
static const u32 zx279133_pon_serdes_sgmii_profile[] = {
	0xe0000004, 0x4fa840a3, 0x013e860f, 0x0210c073,
	0x00000048, 0x00000000, 0x00ff8000, 0x00238620,
	0x80000000, 0x00050000, 0x0c633830, 0x00000007,
	0x00000020, 0x00002000, 0x00000000, 0x00ff0000,
	0xc5a85100, 0x00000000, 0x01045b2a, 0x66002200,
	0x34000003, 0x00000100, 0x00000080, 0x00000000,
	0x200554a8, 0x00648091, 0x01000000, 0x00003700,
	0xa9004000, 0x10640002, 0x000000da, 0xf20000c8,
	0x10371037, 0x01000200, 0x00020001, 0x10000000,
	0x0000001c, 0x8000c000, 0x000000ff, 0x55555500,
	0x55555555, 0x00555555, 0x30000818, 0x40002000,
	0x0000000c, 0x01000000, 0x00000080, 0x00010000,
	0x00000000,
};

static_assert(ARRAY_SIZE(zx279133_pon_serdes_2500basex_profile) ==
	      ZX279133_PON_SERDES_PROFILE_WORDS);
static_assert(ARRAY_SIZE(zx279133_pon_serdes_sgmii_profile) ==
	      ZX279133_PON_SERDES_PROFILE_WORDS);

static const u32 *zx279133_pon_serdes_profile(phy_interface_t interface)
{
	switch (interface) {
	case PHY_INTERFACE_MODE_SGMII:
		return zx279133_pon_serdes_sgmii_profile;
	case PHY_INTERFACE_MODE_2500BASEX:
		return zx279133_pon_serdes_2500basex_profile;
	default:
		return NULL;
	}
}

static const char *zx279133_pon_serdes_mode_name(phy_interface_t interface)
{
	switch (interface) {
	case PHY_INTERFACE_MODE_SGMII:
		return "SGMII";
	case PHY_INTERFACE_MODE_2500BASEX:
		return "2.5GBASE-X";
	default:
		return "unknown";
	}
}

static void zx279133_pon_serdes_write_profile(struct zx279133_pon_serdes *priv,
					      const u32 *profile, size_t count)
{
	unsigned int i;

	for (i = 0; i < count; i++)
		writel(profile[i], priv->serdes + i * sizeof(u32));

	writel((readl(priv->serdes + ZX279133_PON_SERDES_MODE_CTRL) &
		~ZX279133_PON_SERDES_MODE_MASK) | ZX279133_PON_SERDES_MODE_EN,
	       priv->serdes + ZX279133_PON_SERDES_MODE_CTRL);
	writel(readl(priv->serdes + ZX279133_PON_SERDES_ACTIVATE0) |
	       ZX279133_PON_SERDES_ACTIVATE0_EN,
	       priv->serdes + ZX279133_PON_SERDES_ACTIVATE0);
	writel(readl(priv->serdes + ZX279133_PON_SERDES_ACTIVATE1) |
	       ZX279133_PON_SERDES_ACTIVATE1_EN,
	       priv->serdes + ZX279133_PON_SERDES_ACTIVATE1);
}

static int zx279133_pon_serdes_configure_pll(struct zx279133_pon_serdes *priv)
{
	u32 val;
	int ret;

	ret = regmap_read(priv->topcrm, ZX279133_TOPCRM_PON_MUX_CTRL,
			  &priv->saved_mux);
	if (ret)
		return ret;

	ret = regmap_read(priv->topcrm, ZX279133_TOPCRM_PON_PLL_SEL,
			  &priv->saved_pll_sel);
	if (ret)
		return ret;

	priv->saved_pll_cfg0 =
		readl(priv->topcrm_mode + ZX279133_PON_PLL_CFG0);
	priv->saved_pll_cfg1 =
		readl(priv->topcrm_mode + ZX279133_PON_PLL_CFG1);
	priv->configured = true;

	ret = regmap_update_bits(priv->topcrm, ZX279133_TOPCRM_PON_PLL_SEL,
				 ZX279133_TOPCRM_PON_PLL_SEL_MASK, 0);
	if (ret)
		return ret;

	writel(readl(priv->topcrm_mode + ZX279133_PON_PLL_CFG1) |
	       ZX279133_PON_PLL_CFG1_LATCH,
	       priv->topcrm_mode + ZX279133_PON_PLL_CFG1);
	val = readl(priv->topcrm_mode + ZX279133_PON_PLL_CFG0);
	val &= ZX279133_PON_PLL_CFG0_FW_BIT;
	val |= ZX279133_PON_PLL_CFG0_ETH;
	writel(val, priv->topcrm_mode + ZX279133_PON_PLL_CFG0);
	writel(ZX279133_PON_PLL_CFG1_ETH,
	       priv->topcrm_mode + ZX279133_PON_PLL_CFG1);

	return regmap_update_bits(priv->topcrm, ZX279133_TOPCRM_PON_MUX_CTRL,
				  ZX279133_TOPCRM_PON_MODE_MASK,
				  ZX279133_TOPCRM_PON_ETH_SEL);
}

static void zx279133_pon_serdes_restore_pll(struct zx279133_pon_serdes *priv)
{
	if (!priv->configured)
		return;

	regmap_update_bits(priv->topcrm, ZX279133_TOPCRM_PON_MUX_CTRL,
			   ZX279133_TOPCRM_PON_MODE_MASK,
			   priv->saved_mux & ZX279133_TOPCRM_PON_MODE_MASK);
	regmap_update_bits(priv->topcrm, ZX279133_TOPCRM_PON_PLL_SEL,
			   ZX279133_TOPCRM_PON_PLL_SEL_MASK,
			   priv->saved_pll_sel & ZX279133_TOPCRM_PON_PLL_SEL_MASK);
	writel(priv->saved_pll_cfg0,
	       priv->topcrm_mode + ZX279133_PON_PLL_CFG0);
	writel(priv->saved_pll_cfg1,
	       priv->topcrm_mode + ZX279133_PON_PLL_CFG1);
	priv->configured = false;
}

static int zx279133_pon_serdes_reset(struct zx279133_pon_serdes *priv)
{
	int ret;

	ret = reset_control_bulk_assert(ARRAY_SIZE(priv->resets), priv->resets);
	if (ret)
		return ret;

	usleep_range(ZX279133_PON_SERDES_RESET_US,
		     ZX279133_PON_SERDES_RESET_US + 1000);

	ret = reset_control_bulk_deassert(ARRAY_SIZE(priv->resets), priv->resets);
	if (ret) {
		reset_control_bulk_assert(ARRAY_SIZE(priv->resets), priv->resets);
		return ret;
	}

	usleep_range(ZX279133_PON_SERDES_RESET_US,
		     ZX279133_PON_SERDES_RESET_US + 1000);
	return 0;
}

static int zx279133_pon_serdes_set_mode(struct phy *phy, enum phy_mode mode,
					int submode)
{
	struct zx279133_pon_serdes *priv = phy_get_drvdata(phy);

	if (mode != PHY_MODE_ETHERNET)
		return -EOPNOTSUPP;

	if (submode != PHY_INTERFACE_MODE_2500BASEX &&
	    submode != PHY_INTERFACE_MODE_SGMII)
		return -EOPNOTSUPP;

	mutex_lock(&priv->lock);
	priv->interface = submode;
	mutex_unlock(&priv->lock);

	return 0;
}

static int zx279133_pon_serdes_power_on(struct phy *phy)
{
	struct zx279133_pon_serdes *priv = phy_get_drvdata(phy);
	const u32 *profile;
	u32 val;
	int ret;

	mutex_lock(&priv->lock);
	if (priv->powered) {
		ret = 0;
		goto out_unlock;
	}

	profile = zx279133_pon_serdes_profile(priv->interface);
	if (!profile) {
		dev_err(priv->dev, "unsupported SerDes interface %s\n",
			phy_modes(priv->interface));
		ret = -EINVAL;
		goto out_unlock;
	}

	ret = clk_prepare_enable(priv->pclk);
	if (ret)
		goto out_unlock;

	ret = zx279133_pon_serdes_configure_pll(priv);
	if (ret)
		goto err_disable_clk;

	ret = zx279133_pon_serdes_reset(priv);
	if (ret)
		goto err_disable_clk;

	zx279133_pon_serdes_write_profile(priv, profile,
					  ZX279133_PON_SERDES_PROFILE_WORDS);

	ret = readl_poll_timeout(priv->serdes + ZX279133_PON_SERDES_PLL_STATUS,
				 val, val & ZX279133_PON_SERDES_PLL_LOCK,
				 ZX279133_PON_SERDES_POLL_US,
				 ZX279133_PON_SERDES_LOCK_TIMEOUT_US);
	if (ret)
		dev_warn(priv->dev,
			 "common PLL status did not assert in %s mode\n",
			 zx279133_pon_serdes_mode_name(priv->interface));

	val = readl(priv->serdes + ZX279133_PON_SERDES_RX_STATUS);
	if (!(val & ZX279133_PON_SERDES_RX_LOS)) {
		ret = readl_poll_timeout(priv->serdes + ZX279133_PON_SERDES_RX_STATUS,
					 val, val & ZX279133_PON_SERDES_CDR_LOCK,
					 ZX279133_PON_SERDES_POLL_US,
					 ZX279133_PON_SERDES_LOCK_TIMEOUT_US);
		if (ret) {
			dev_err(priv->dev, "CDR did not lock with RX signal present\n");
			goto err_assert_reset;
		}
	}

	dev_info(priv->dev, "%s SerDes ready, status %#x/%#x\n",
		 zx279133_pon_serdes_mode_name(priv->interface),
		 readl(priv->serdes + ZX279133_PON_SERDES_PLL_STATUS),
		 readl(priv->serdes + ZX279133_PON_SERDES_RX_STATUS));
	priv->powered = true;
	ret = 0;
	goto out_unlock;

err_assert_reset:
	reset_control_bulk_assert(ARRAY_SIZE(priv->resets), priv->resets);
err_disable_clk:
	zx279133_pon_serdes_restore_pll(priv);
	clk_disable_unprepare(priv->pclk);
out_unlock:
	mutex_unlock(&priv->lock);
	return ret;
}

static int zx279133_pon_serdes_power_off(struct phy *phy)
{
	struct zx279133_pon_serdes *priv = phy_get_drvdata(phy);
	int ret = 0;

	mutex_lock(&priv->lock);
	if (!priv->powered)
		goto out_unlock;

	ret = reset_control_bulk_assert(ARRAY_SIZE(priv->resets), priv->resets);
	if (ret)
		goto out_unlock;

	zx279133_pon_serdes_restore_pll(priv);
	clk_disable_unprepare(priv->pclk);
	priv->powered = false;

out_unlock:
	mutex_unlock(&priv->lock);
	return ret;
}

static void zx279133_pon_serdes_cleanup(void *data)
{
	struct zx279133_pon_serdes *priv = data;

	mutex_lock(&priv->lock);
	if (priv->powered) {
		reset_control_bulk_assert(ARRAY_SIZE(priv->resets), priv->resets);
		zx279133_pon_serdes_restore_pll(priv);
		clk_disable_unprepare(priv->pclk);
		priv->powered = false;
	}
	mutex_unlock(&priv->lock);
}

static const struct phy_ops zx279133_pon_serdes_ops = {
	.set_mode = zx279133_pon_serdes_set_mode,
	.power_on = zx279133_pon_serdes_power_on,
	.power_off = zx279133_pon_serdes_power_off,
	.owner = THIS_MODULE,
};

static int zx279133_pon_serdes_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct zx279133_pon_serdes *priv;
	struct phy *phy;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	mutex_init(&priv->lock);

	priv->serdes = devm_platform_ioremap_resource_byname(pdev, "serdes");
	if (IS_ERR(priv->serdes))
		return PTR_ERR(priv->serdes);

	priv->pll = devm_platform_ioremap_resource_byname(pdev, "pll");
	if (IS_ERR(priv->pll))
		return PTR_ERR(priv->pll);

	priv->topcrm_mode =
		devm_platform_ioremap_resource_byname(pdev, "topcrm-mode");
	if (IS_ERR(priv->topcrm_mode))
		return PTR_ERR(priv->topcrm_mode);

	priv->topcrm = syscon_regmap_lookup_by_phandle(dev->of_node, "zte,topcrm");
	if (IS_ERR(priv->topcrm))
		return dev_err_probe(dev, PTR_ERR(priv->topcrm),
				     "failed to get TOPCRM syscon\n");

	priv->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(priv->pclk))
		return dev_err_probe(dev, PTR_ERR(priv->pclk),
				     "failed to get PCLK\n");

	priv->resets[0].id = "serdes";
	priv->resets[1].id = "apb";
	ret = devm_reset_control_bulk_get_exclusive(dev, ARRAY_SIZE(priv->resets),
						    priv->resets);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get resets\n");

	ret = devm_add_action_or_reset(dev, zx279133_pon_serdes_cleanup, priv);
	if (ret)
		return ret;

	phy = devm_phy_create(dev, NULL, &zx279133_pon_serdes_ops);
	if (IS_ERR(phy))
		return dev_err_probe(dev, PTR_ERR(phy), "failed to create PHY\n");

	phy_set_drvdata(phy, priv);
	platform_set_drvdata(pdev, priv);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	return PTR_ERR_OR_ZERO(provider);
}

static const struct of_device_id zx279133_pon_serdes_of_match[] = {
	{ .compatible = "zte,zx279133-pon-serdes" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_pon_serdes_of_match);

static struct platform_driver zx279133_pon_serdes_driver = {
	.probe = zx279133_pon_serdes_probe,
	.driver = {
		.name = "zx279133-pon-serdes",
		.of_match_table = zx279133_pon_serdes_of_match,
	},
};
module_platform_driver(zx279133_pon_serdes_driver);

MODULE_DESCRIPTION("ZTE ZX279133 PON SerDes PHY driver");
MODULE_LICENSE("GPL");
