// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mdio.h>
#include <linux/mii.h>
#include <linux/pcs/pcs-xpcs.h>
#include <linux/phy.h>
#include <linux/phy/phy.h>
#include <linux/phylink.h>

#include "zx279133.h"

static struct phylink_pcs *
zx279133_mac_select_pcs(struct phylink_config *config,
			phy_interface_t interface)
{
	struct zx279133_eth *eth = container_of(config, struct zx279133_eth,
					       phylink_config);

	if (interface != PHY_INTERFACE_MODE_2500BASEX &&
	    interface != PHY_INTERFACE_MODE_SGMII)
		return ERR_PTR(-EOPNOTSUPP);

	return xpcs_to_phylink_pcs(eth->xpcs);
}

void zx279133_xmac_set_enabled(struct zx279133_eth *eth, bool enabled)
{
	void __iomem *xmac = eth->base + ZX279133_XMAC1_BASE;
	u32 value;

	value = readl(xmac + ZX279133_XMAC_RX_CTRL);
	writel(enabled ? value | ZX279133_XMAC_ENABLE :
			 value & ~ZX279133_XMAC_ENABLE,
	       xmac + ZX279133_XMAC_RX_CTRL);

	value = readl(xmac + ZX279133_XMAC_TX_CTRL);
	writel(enabled ? value | ZX279133_XMAC_ENABLE :
			 value & ~ZX279133_XMAC_ENABLE,
	       xmac + ZX279133_XMAC_TX_CTRL);
}

/*
 * Vendor-Linux XPCS SGMII bring-up for ZX279133, recovered from
 * xpcs_1g_mode_conf() and xpcs_auto_negotiation_conf_in_sgmii_mode() in
 * plat_132.ko. The generic dw-xpcs C37 SGMII programming alone does not
 * complete AN on this SoC: the PCS-type, XAUI, speed-select, low-power/
 * PSEQ, and SR-MII speed/duplex writes are required. The vendor's initial
 * mode-3 path leaves the link timer and CL37 override at their reset values.
 * C45 registers are accessed through the XPCS MDIO device.
 */
#define ZX279133_XPCS_VR_MII_DIG_CTRL1	0x8000
#define ZX279133_XPCS_VR_MII_AN_CTRL	0x8001
#define ZX279133_XPCS_VR_MII_LINK_TIMER	0x800a
#define ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1 0x8000
#define ZX279133_XPCS_VR_XS_PCS_XAUI_CTRL 0x8004
#define ZX279133_XPCS_VR_XS_PCS_DIG_STS	0x8010
#define ZX279133_XPCS_MAC_AUTO_SW	BIT(9)
#define ZX279133_XPCS_CL37_TMR_OVR	BIT(3)
#define ZX279133_XPCS_2G5_EN		BIT(2)
#define ZX279133_XPCS_AN_INTR_EN	BIT(0)
#define ZX279133_XPCS_SGMII_LINK_STS	BIT(4)
#define ZX279133_XPCS_PCS_MODE_MASK	GENMASK(2, 1)
#define ZX279133_XPCS_LOW_POWER		BIT(11)
#define ZX279133_XPCS_SPEED_SEL		BIT(13)
#define ZX279133_XPCS_SR_MII_SPEED_MASK (BIT(13) | BIT(6) | BIT(5))
#define ZX279133_XPCS_DUPLEX		BIT(8)
#define ZX279133_XPCS_LINK_TIMER_VALUE	1953
#define ZX279133_XPCS_PSEQ_MASK		GENMASK(4, 2)
#define ZX279133_XPCS_PSEQ_IDLE		0x4

/*
 * Use the dw-xpcs platform MDIO device. Earlier bring-up used the external
 * ZX279133 MDIO controller by mistake; those C45 writes never reached the
 * 0x1b000000 window. mdiodev_c45_* hits pcs-xpcs-plat's direct MMIO path.
 */
static int zx279133_xpcs_read(struct zx279133_eth *eth, int devad, int reg)
{
	return mdiodev_c45_read(eth->xpcs_mdiodev, devad, reg);
}

static int zx279133_xpcs_write(struct zx279133_eth *eth, int devad, int reg,
			       u16 val)
{
	return mdiodev_c45_write(eth->xpcs_mdiodev, devad, reg, val);
}

static int zx279133_xpcs_modify(struct zx279133_eth *eth, int devad, int reg,
				u16 mask, u16 set)
{
	return mdiodev_c45_modify(eth->xpcs_mdiodev, devad, reg, mask, set);
}

static int zx279133_xpcs_init(struct zx279133_eth *eth)
{
	int val;
	int ret;

	ret = read_poll_timeout(zx279133_xpcs_read, val,
				val < 0 || !(val & BIT(15)), 859, 859 * 400,
				false, eth, MDIO_MMD_PCS, MII_BMCR);
	if (ret)
		return ret;
	if (val < 0)
		return val;

	ret = read_poll_timeout(zx279133_xpcs_read, val,
				val < 0 || !(val & BIT(15)), 859, 859 * 400,
				false, eth, MDIO_MMD_VEND2, MII_BMCR);
	if (ret)
		return ret;
	if (val < 0)
		return val;

	return zx279133_xpcs_modify(eth, MDIO_MMD_VEND2,
				      ZX279133_XPCS_VR_MII_AN_CTRL,
				      BIT(3) | BIT(8), 0);
}

static int zx279133_xpcs_apply_sgmii_vend2(struct zx279133_eth *eth,
					   int duplex)
{
	u32 bmcr = BMCR_ANENABLE | BMCR_SPEED1000;
	int ret;

	if (duplex != DUPLEX_HALF)
		bmcr |= BMCR_FULLDPLX;

	ret = zx279133_xpcs_write(eth, MDIO_MMD_VEND2,
				  ZX279133_XPCS_VR_MII_AN_CTRL,
				 ZX279133_XPCS_SGMII_LINK_STS |
				 ZX279133_XPCS_AN_INTR_EN |
				 FIELD_PREP(ZX279133_XPCS_PCS_MODE_MASK, 2));
	if (ret)
		return ret;

	ret = zx279133_xpcs_write(eth, MDIO_MMD_VEND2,
				  ZX279133_XPCS_VR_MII_DIG_CTRL1,
				 0x2000 | ZX279133_XPCS_MAC_AUTO_SW);
	if (ret)
		return ret;

	return zx279133_xpcs_write(eth, MDIO_MMD_VEND2, MII_BMCR, bmcr);
}

static int zx279133_xpcs_sgmii_quirk(struct zx279133_eth *eth)
{
	int val;
	int ret;

	ret = zx279133_xpcs_init(eth);
	if (ret)
		return ret;

	/* xpcs_exit_sgmii_mode(): clear any previous mode state. */
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_VEND2, MII_BMCR,
				   BMCR_ANENABLE, 0);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_VEND2,
				   ZX279133_XPCS_VR_MII_AN_CTRL,
				  ZX279133_XPCS_AN_INTR_EN |
				  ZX279133_XPCS_SGMII_LINK_STS, 0);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_VEND2,
				   ZX279133_XPCS_VR_MII_DIG_CTRL1,
				  ZX279133_XPCS_MAC_AUTO_SW |
				  ZX279133_XPCS_2G5_EN, 0);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PCS,
				   ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1,
				  ZX279133_XPCS_2G5_EN, 0);
	if (ret)
		return ret;

	/* xpcs_1g_mode_conf(xmac, speed=3, duplex=1, pcs_mode=2). */
	ret = zx279133_xpcs_write(eth, MDIO_MMD_PCS, 7, 1);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PCS,
				   ZX279133_XPCS_VR_XS_PCS_XAUI_CTRL,
				  BIT(0), 0);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PMAPMD, MII_BMCR,
				   ZX279133_XPCS_SPEED_SEL, 0);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PCS, MII_BMCR,
				   ZX279133_XPCS_SPEED_SEL, 0);
	if (ret)
		return ret;
	/* SGMII SerDes stays 1.25G; 10/100/1000 are in-band. Vendor
	 * xpcs_sgmii_mode_conf() always programs SR-MII speed 3 / full.
	 */
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_VEND2, MII_BMCR,
				   ZX279133_XPCS_SR_MII_SPEED_MASK, BIT(6));
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_VEND2, MII_BMCR,
				   ZX279133_XPCS_DUPLEX, ZX279133_XPCS_DUPLEX);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PCS, MII_BMCR,
				   ZX279133_XPCS_LOW_POWER,
				  ZX279133_XPCS_LOW_POWER);
	if (ret)
		return ret;

	ret = read_poll_timeout(zx279133_xpcs_read, val,
				val < 0 ||
				FIELD_GET(ZX279133_XPCS_PSEQ_MASK, val) !=
				ZX279133_XPCS_PSEQ_IDLE,
				859, 859 * 400, false, eth, MDIO_MMD_PCS,
				ZX279133_XPCS_VR_XS_PCS_DIG_STS);
	if (ret)
		return ret;
	if (val < 0)
		return val;

	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PCS, MII_BMCR,
				   ZX279133_XPCS_LOW_POWER, 0);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_VEND2,
				   ZX279133_XPCS_VR_MII_AN_CTRL,
				  ZX279133_XPCS_PCS_MODE_MASK,
				  FIELD_PREP(ZX279133_XPCS_PCS_MODE_MASK, 2));
	if (ret)
		return ret;

	/*
	 * xmac_init_by_work_mode(3) calls xmac_sgmii_conf(..., auto=0).
	 * After 2.5GBASE-X, ANENABLE will not stick; use the vendor
	 * AN-disabled path: PCS mode 2 + SGMII_LINK_STS, no MAC_AUTO_SW.
	 */
	ret = zx279133_xpcs_write(eth, MDIO_MMD_VEND2,
				  ZX279133_XPCS_VR_MII_AN_CTRL,
				 ZX279133_XPCS_SGMII_LINK_STS |
				 FIELD_PREP(ZX279133_XPCS_PCS_MODE_MASK, 2));
	if (ret)
		return ret;
	ret = zx279133_xpcs_write(eth, MDIO_MMD_VEND2,
				  ZX279133_XPCS_VR_MII_DIG_CTRL1, 0x2000);
	if (ret)
		return ret;

	return zx279133_xpcs_write(eth, MDIO_MMD_VEND2, MII_BMCR,
				     BMCR_SPEED1000 | BMCR_FULLDPLX);
}

static int zx279133_xpcs_2500basex_quirk(struct zx279133_eth *eth)
{
	int ret;

	ret = zx279133_xpcs_init(eth);
	if (ret)
		return ret;

	/*
	 * Vendor xpcs_2p5gbase_x_conf(): program PCS type 0xe and pulse
	 * PMA low power. The fixed VEND2 image is the live vendor-Linux
	 * 2.5G state after the mode-3-to-mode-4 transition.
	 */
	ret = zx279133_xpcs_write(eth, MDIO_MMD_PCS, 7, 0x0e);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PMAPMD, MII_BMCR,
				   ZX279133_XPCS_SPEED_SEL, 0);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PMAPMD, MII_BMCR,
				   ZX279133_XPCS_LOW_POWER,
				  ZX279133_XPCS_LOW_POWER);
	if (ret)
		return ret;
	usleep_range(859, 959);
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_PMAPMD, MII_BMCR,
				   ZX279133_XPCS_LOW_POWER, 0);
	if (ret)
		return ret;

	ret = zx279133_xpcs_modify(eth, MDIO_MMD_VEND2, MII_BMCR,
				   BMCR_ANENABLE | BMCR_SPEED1000 | BMCR_SPEED100 |
				  BMCR_FULLDPLX,
				  BMCR_SPEED1000 | BMCR_FULLDPLX);
	if (ret)
		return ret;
	ret = zx279133_xpcs_modify(eth, MDIO_MMD_VEND2,
				   ZX279133_XPCS_VR_MII_DIG_CTRL1,
				  ZX279133_XPCS_MAC_AUTO_SW |
				  ZX279133_XPCS_2G5_EN | BIT(13), BIT(13));
	if (ret)
		return ret;

	return zx279133_xpcs_modify(eth, MDIO_MMD_VEND2,
				      ZX279133_XPCS_VR_MII_AN_CTRL,
				      ZX279133_XPCS_AN_INTR_EN |
				      ZX279133_XPCS_SGMII_LINK_STS |
				      ZX279133_XPCS_PCS_MODE_MASK,
				      FIELD_PREP(ZX279133_XPCS_PCS_MODE_MASK, 2));
}

int zx279133_xpcs_set_bypass(struct zx279133_eth *eth, bool enabled)
{
	int value;

	value = zx279133_xpcs_read(eth, MDIO_MMD_PCS, ZX279133_XPCS_BYPASS_REG);
	if (value < 0)
		return value;

	if (enabled)
		value |= ZX279133_XPCS_BYPASS_EN;
	else
		value &= ~ZX279133_XPCS_BYPASS_EN;

	return zx279133_xpcs_write(eth, MDIO_MMD_PCS,
				     ZX279133_XPCS_BYPASS_REG, value);
}

static int zx279133_mac_prepare(struct phylink_config *config,
				unsigned int mode, phy_interface_t interface)
{
	struct zx279133_eth *eth = container_of(config, struct zx279133_eth,
					       phylink_config);
	phy_interface_t previous;
	int restore_ret;
	int ret;

	if (!eth->hardware_prepared || eth->serdes_interface == interface)
		return 0;
	if (interface != PHY_INTERFACE_MODE_2500BASEX &&
	    interface != PHY_INTERFACE_MODE_SGMII)
		return -EOPNOTSUPP;

	previous = eth->serdes_interface;
	zx279133_xmac_set_enabled(eth, false);
	ret = phy_power_off(eth->serdes);
	if (ret) {
		dev_err(eth->dev, "failed to power off SerDes for %s: %d\n",
			phy_modes(interface), ret);
		return ret;
	}
	eth->serdes_powered = false;

	ret = phy_set_mode_ext(eth->serdes, PHY_MODE_ETHERNET, interface);
	if (!ret)
		ret = phy_power_on(eth->serdes);
	if (!ret) {
		eth->serdes_powered = true;
		eth->serdes_interface = interface;
		eth->host_interface = interface;
		usleep_range(2000, 2500);
		return 0;
	}

	restore_ret = phy_set_mode_ext(eth->serdes, PHY_MODE_ETHERNET,
				       previous);
	if (!restore_ret)
		restore_ret = phy_power_on(eth->serdes);
	if (!restore_ret)
		eth->serdes_powered = true;
	else
		eth->serdes_powered = false;
	if (restore_ret) {
		eth->serdes_interface = PHY_INTERFACE_MODE_NA;
		dev_err(eth->dev,
			"failed to restore %s SerDes mode: %d\n",
			phy_modes(previous), restore_ret);
	}

	return ret;
}

static void zx279133_mac_config(struct phylink_config *config,
				unsigned int mode,
			       const struct phylink_link_state *state)
{
	/* XMAC1 is programmed by mac_link_up() once link parameters resolve. */
	if (state->interface != PHY_INTERFACE_MODE_2500BASEX &&
	    state->interface != PHY_INTERFACE_MODE_SGMII)
		return;
}

/*
 * xamc_init_conf_by_speed(): the vendor XMAC1 register image, identical for
 * every mode except the speed code in TX_CTRL bits 31:29
 * (xmac_set_speed_sel(): 7 = 10M, 4 = 100M, 3 = 1G SGMII, 6 = 2.5G).
 */
static u32 zx279133_xmac_speed_code(int speed)
{
	switch (speed) {
	case SPEED_10:
		return 7;
	case SPEED_100:
		return 4;
	case SPEED_1000:
		return ZX279133_XMAC1_SGMII_SPEED;
	case SPEED_2500:
		return 6;
	default:
		return U32_MAX;
	}
}

static void zx279133_xmac_program(struct zx279133_eth *eth, u32 speed_code,
				  int duplex)
{
	void __iomem *xmac = eth->base + ZX279133_XMAC1_BASE;
	u32 value;

	writel(0x00010000, xmac + ZX279133_XMAC_TX_CTRL);
	value = readl(xmac + ZX279133_XMAC_TX_CTRL);
	writel((value & ~ZX279133_XMAC_SPEED_MASK) |
	       FIELD_PREP(ZX279133_XMAC_SPEED_MASK, speed_code),
	       xmac + ZX279133_XMAC_TX_CTRL);

	value = readl(xmac + ZX279133_XMAC_DUPLEX);
	if (duplex == DUPLEX_HALF)
		writel(value | ZX279133_XMAC_HALF_DUPLEX,
		       xmac + ZX279133_XMAC_DUPLEX);
	else
		writel(value & ~ZX279133_XMAC_HALF_DUPLEX,
		       xmac + ZX279133_XMAC_DUPLEX);
	writel(0x3e800086, xmac + ZX279133_XMAC_RX_CTRL);
	writel(0x80000001, xmac + ZX279133_XMAC_FRAME_CFG);
	writel(2, xmac + ZX279133_XMAC_MODE_CFG);

	value = readl(xmac + ZX279133_XMAC_MISC_CFG);
	writel(value | BIT(9), xmac + ZX279133_XMAC_MISC_CFG);
}

/*
 * Vendor xmac_flow_send_ctrl_set(): TFE (id 34, +0x1c0 bit1) honors
 * received pause; PT (id 35, +0x1c0[31:16]) is pause_time; RX_FLOW
 * (id 37, +0x240 bit0) generates pause. phylink rx_pause maps to TFE
 * and tx_pause maps to RX_FLOW.
 */
static void zx279133_xmac_set_pause(struct zx279133_eth *eth,
				    bool tx_pause, bool rx_pause)
{
	void __iomem *xmac = eth->base + ZX279133_XMAC1_BASE;
	u32 flow;

	flow = readl(xmac + ZX279133_XMAC_FLOW_CTRL);
	if (rx_pause)
		flow |= ZX279133_XMAC_TFE;
	else
		flow &= ~ZX279133_XMAC_TFE;
	flow &= ~ZX279133_XMAC_PT_MASK;
	if (tx_pause || rx_pause)
		flow |= FIELD_PREP(ZX279133_XMAC_PT_MASK,
				  ZX279133_XMAC_PAUSE_TIME);
	writel(flow, xmac + ZX279133_XMAC_FLOW_CTRL);

	flow = readl(xmac + ZX279133_XMAC_RX_FLOW);
	if (tx_pause)
		flow |= ZX279133_XMAC_RX_FLOW_EN;
	else
		flow &= ~ZX279133_XMAC_RX_FLOW_EN;
	writel(flow, xmac + ZX279133_XMAC_RX_FLOW);
}

static void zx279133_xmac_reset(struct zx279133_eth *eth)
{
	void __iomem *reset = eth->base + ZX279133_SYS_SOFT_RESET;
	u32 value;

	/* smac_reset(): preserve the shared reset word and pulse XMAC1 only. */
	value = readl(reset);
	writel(value & ~ZX279133_XMAC_RESET_MASK, reset);
	/* Vendor smac_reset(): __const_udelay(1718000) ~= 1.718 ms. */
	usleep_range(1718, 1818);
	writel(value | ZX279133_XMAC_RESET_MASK, reset);
}

static int zx279133_xmac_2500basex_init(struct zx279133_eth *eth)
{
	u32 value;
	int ret;

	/* xmac_init_by_work_mode(4): reset/configure, then publish SOPC. */
	zx279133_xmac_set_enabled(eth, false);
	writel(0, eth->base + ZX279133_XMAC1_SOPC_SEND_ENABLE);

	ret = zx279133_xpcs_init(eth);
	if (ret)
		return ret;
	zx279133_xmac_reset(eth);
	ret = zx279133_xpcs_2500basex_quirk(eth);
	if (ret)
		return ret;
	ret = zx279133_xpcs_set_bypass(eth, true);
	if (ret)
		return ret;

	zx279133_xmac_program(eth, 6, DUPLEX_FULL);
	value = readl(eth->base + 0x343f0);
	writel(value & ~ZX279133_XMAC1_SOPC_DUPLEX_MASK,
	       eth->base + 0x343f0);
	usleep_range(4295, 4395);
	writel(1, eth->base + ZX279133_XMAC1_SOPC_SEND_ENABLE);
	zx279133_xmac_set_enabled(eth, true);

	return 0;
}

static int zx279133_xmac_sgmii_init(struct zx279133_eth *eth, int speed,
				    int duplex)
{
	u32 value;
	u32 speed_code;
	int ret;

	speed_code = zx279133_xmac_speed_code(speed);
	if (speed_code == U32_MAX)
		return -EOPNOTSUPP;

	/* xmac_init_by_work_mode(3): reset/configure, then publish SOPC enable. */
	zx279133_xmac_set_enabled(eth, false);
	writel(0, eth->base + ZX279133_XMAC1_SOPC_SEND_ENABLE);
	zx279133_xmac_reset(eth);

	ret = zx279133_xpcs_sgmii_quirk(eth);
	if (ret)
		return ret;

	ret = zx279133_xpcs_set_bypass(eth, true);
	if (ret)
		return ret;

	zx279133_xmac_program(eth, speed_code, duplex);

	value = readl(eth->base + 0x343f0);
	if (duplex == DUPLEX_HALF)
		writel(value | ZX279133_XMAC1_SOPC_DUPLEX_MASK,
		       eth->base + 0x343f0);
	else
		writel(value & ~ZX279133_XMAC1_SOPC_DUPLEX_MASK,
		       eth->base + 0x343f0);
	/* Vendor xmac_init_by_work_mode(): __const_udelay(4295000). */
	usleep_range(4295, 4395);
	writel(1, eth->base + ZX279133_XMAC1_SOPC_SEND_ENABLE);
	zx279133_xmac_set_enabled(eth, true);
	/* Re-apply after XMAC/SOPC; C45 AN bits are otherwise lost. */
	ret = zx279133_xpcs_apply_sgmii_vend2(eth, duplex);
	if (ret) {
		zx279133_xmac_set_enabled(eth, false);
		writel(0, eth->base + ZX279133_XMAC1_SOPC_SEND_ENABLE);
		return ret;
	}

	return 0;
}

static int zx279133_mac_finish(struct phylink_config *config,
			       unsigned int mode, phy_interface_t interface)
{
	struct zx279133_eth *eth = container_of(config, struct zx279133_eth,
					       phylink_config);

	if (interface != PHY_INTERFACE_MODE_2500BASEX &&
	    interface != PHY_INTERFACE_MODE_SGMII)
		return -EOPNOTSUPP;

	/* The mode-specific image is applied in mac_link_up(), after PCS link-up. */
	return zx279133_xpcs_set_bypass(eth, true);
}

static void zx279133_mac_link_down(struct phylink_config *config,
				   unsigned int mode,
				  phy_interface_t interface)
{
	struct zx279133_eth *eth = container_of(config, struct zx279133_eth,
					       phylink_config);

	mutex_lock(&eth->xmac_lock);
	if (eth->hardware_prepared)
		zx279133_xmac_set_enabled(eth, false);
	mutex_unlock(&eth->xmac_lock);
}

static void zx279133_mac_link_up(struct phylink_config *config,
				 struct phy_device *phy, unsigned int mode,
				phy_interface_t interface, int speed, int duplex,
				bool tx_pause, bool rx_pause)
{
	struct zx279133_eth *eth = container_of(config, struct zx279133_eth,
					       phylink_config);
	int ret;

	mutex_lock(&eth->xmac_lock);
	if (!eth->hardware_prepared)
		goto out_unlock;

	if (interface == PHY_INTERFACE_MODE_SGMII) {
		if (speed != SPEED_10 && speed != SPEED_100 &&
		    speed != SPEED_1000) {
			dev_info(eth->dev,
				 "unsupported SGMII link speed %d, XMAC not enabled\n",
				 speed);
			goto out_unlock;
		}

		ret = zx279133_xmac_sgmii_init(eth, speed, duplex);
		if (ret) {
			dev_err(eth->dev,
				"SGMII XMAC initialization failed: %d\n", ret);
			goto out_unlock;
		}
	} else {
		if (speed != SPEED_2500) {
			dev_info(eth->dev,
				 "unsupported 2500BASE-X link speed %d, XMAC not enabled\n",
				 speed);
			goto out_unlock;
		}

		ret = zx279133_xmac_2500basex_init(eth);
		if (ret) {
			dev_err(eth->dev,
				"2.5GBASE-X XMAC initialization failed: %d\n", ret);
			goto out_unlock;
		}
	}

	zx279133_xmac_set_pause(eth, tx_pause, rx_pause);

	if (zx279133_wan_port_bringup(eth))
		dev_err(eth->dev, "WAN port bring-up failed\n");

out_unlock:
	mutex_unlock(&eth->xmac_lock);
}

static void zx279133_mac_disable_tx_lpi(struct phylink_config *config)
{
	struct zx279133_eth *eth = container_of(config, struct zx279133_eth,
					       phylink_config);
	u32 val;

	/* XMAC1 programming recovered from factory xmac_eee_conf(1, enable). */
	mutex_lock(&eth->xmac_lock);
	val = readl(eth->base + ZX279133_XMAC1_EEE_CTRL);
	writel(val & ~ZX279133_XMAC_EEE_ENABLE_MASK,
	       eth->base + ZX279133_XMAC1_EEE_CTRL);
	writel(readl(eth->base + ZX279133_XMAC1_EEE_TIMER) &
	       ~ZX279133_XMAC_EEE_TIMER_ENABLE,
	       eth->base + ZX279133_XMAC1_EEE_TIMER);
	mutex_unlock(&eth->xmac_lock);
}

static int zx279133_mac_enable_tx_lpi(struct phylink_config *config, u32 timer,
				      bool tx_clk_stop)
{
	struct zx279133_eth *eth = container_of(config, struct zx279133_eth,
					       phylink_config);
	u32 val;

	if (timer > FIELD_MAX(ZX279133_XMAC_EEE_TIMER_MASK))
		return -EINVAL;

	mutex_lock(&eth->xmac_lock);
	val = readl(eth->base + ZX279133_XMAC1_EEE_CTRL);
	writel(val | ZX279133_XMAC_EEE_ENABLE_MASK,
	       eth->base + ZX279133_XMAC1_EEE_CTRL);
	writel(FIELD_PREP(ZX279133_XMAC_EEE_TIMER_MASK, timer) |
	       ZX279133_XMAC_EEE_TIMER_ENABLE,
	       eth->base + ZX279133_XMAC1_EEE_TIMER);
	mutex_unlock(&eth->xmac_lock);
	return 0;
}

const struct phylink_mac_ops zx279133_phylink_ops = {
	.mac_select_pcs = zx279133_mac_select_pcs,
	.mac_prepare = zx279133_mac_prepare,
	.mac_config = zx279133_mac_config,
	.mac_finish = zx279133_mac_finish,
	.mac_link_down = zx279133_mac_link_down,
	.mac_link_up = zx279133_mac_link_up,
	.mac_disable_tx_lpi = zx279133_mac_disable_tx_lpi,
	.mac_enable_tx_lpi = zx279133_mac_enable_tx_lpi,
};
