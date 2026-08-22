// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/mdio.h>
#include <linux/mii.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/slab.h>

#define ZX279051_PHY_ID		0x000084b9
#define ZX279051_ID_DEVAD	MDIO_MMD_AN
#define ZX279051_ID_REG		MDIO_DEVID1
#define ZX279051_GE_STATUS	26
#define ZX279051_GE_LINK		BIT(6)
#define ZX279051_GE_SPEED	GENMASK(9, 7)

#define ZX279051_TEMP_STATUS_REG	0xb626
#define ZX279051_TEMP_VALID		5
#define ZX279051_TEMP_VALUE_REG		0x002e
#define ZX279051_CFCTRL_REG		0x805a
#define ZX279051_CFCTRL_FIELD		GENMASK(9, 3)
#define ZX279051_CFCTRL_APPLY_REG	0xb616
#define ZX279051_ANALOG_GATE_REG	0xb60f
#define ZX279051_TEMP_TRIGGER_REG	0xb62a

#define ZX279051_INIT_C22	0
#define ZX279051_INIT_C45	1

struct zx279051_init_write {
	u8 access;
	u8 devad;
	u16 reg;
	u16 val;
};

#include "zx279051-fullmask.h"

static_assert(ARRAY_SIZE(zx279051_fullmask_writes) == 117);

struct zx279051_priv {
	phy_interface_t host_interface;
	unsigned int host_speed_code;
};

static int zx279051_read_mmd(struct phy_device *phydev, int devad, u16 regnum)
{
	return mdiobus_c45_read(phydev->mdio.bus, phydev->mdio.addr,
				 devad, regnum);
}

static int zx279051_write_mmd(struct phy_device *phydev, int devad,
			      u16 regnum, u16 val)
{
	return mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				  devad, regnum, val);
}

static int zx279051_write_c22(struct phy_device *phydev, u16 regnum, u16 val)
{
	return mdiobus_write(phydev->mdio.bus, phydev->mdio.addr, regnum, val);
}

static int zx279051_fullmask_write(struct phy_device *phydev,
				   const struct zx279051_init_write *write)
{
	if (write->access == ZX279051_INIT_C22)
		return zx279051_write_c22(phydev, write->reg, write->val);

	return zx279051_write_mmd(phydev, write->devad, write->reg,
				   write->val);
}

static int zx279051_fullmask_temperature_adjust(struct phy_device *phydev)
{
	unsigned int cfctrl;
	int status, temp;
	int ret;

	ret = zx279051_write_mmd(phydev, MDIO_MMD_VEND1,
				 ZX279051_TEMP_TRIGGER_REG, 0);
	if (ret)
		return ret;
	ret = zx279051_write_c22(phydev, MII_BMCR, 0);
	if (ret)
		return ret;
	ret = zx279051_write_mmd(phydev, MDIO_MMD_VEND1,
				 ZX279051_ANALOG_GATE_REG, 1);
	if (ret)
		return ret;
	ret = zx279051_write_mmd(phydev, MDIO_MMD_VEND1,
				 ZX279051_TEMP_TRIGGER_REG, 1);
	if (ret)
		return ret;

	msleep(30);
	status = zx279051_read_mmd(phydev, MDIO_MMD_VEND1,
				   ZX279051_TEMP_STATUS_REG);
	if (status < 0)
		return status;
	/* The vendor sequence only clears the analog gate for a valid sample. */
	if (status != ZX279051_TEMP_VALID)
		return 0;

	temp = zx279051_read_mmd(phydev, MDIO_MMD_VEND2,
				 ZX279051_TEMP_VALUE_REG);
	if (temp < 0)
		return temp;
	ret = zx279051_read_mmd(phydev, MDIO_MMD_VEND2,
				ZX279051_CFCTRL_REG);
	if (ret < 0)
		return ret;

	cfctrl = FIELD_GET(ZX279051_CFCTRL_FIELD, ret);
	if (temp <= 0x6699)
		cfctrl += 6;
	else if (temp <= 0x6f0a)
		cfctrl += 3;
	else if (temp > 0x7350)
		cfctrl--;

	ret = zx279051_write_mmd(phydev, MDIO_MMD_VEND1,
				 ZX279051_CFCTRL_APPLY_REG, cfctrl);
	if (ret)
		return ret;

	return zx279051_write_mmd(phydev, MDIO_MMD_VEND1,
				  ZX279051_ANALOG_GATE_REG, 0);
}

static int zx279051_fullmask_init(struct phy_device *phydev)
{
	int bmcr, an_adv;
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(zx279051_fullmask_writes); i++) {
		ret = zx279051_fullmask_write(phydev,
					      &zx279051_fullmask_writes[i]);
		if (ret) {
			phydev_err(phydev,
				   "post-reset initialization failed at step %u: %pe\n",
				   i + 1, ERR_PTR(ret));
			return ret;
		}
	}

	/* Vendor power/temperature-calibration sequence; delays are required. */
	msleep(200);
	ret = zx279051_write_c22(phydev, MII_BMCR, 0);
	if (ret)
		return ret;
	msleep(100);
	ret = zx279051_write_c22(phydev, MII_BMCR, BMCR_PDOWN);
	if (ret)
		return ret;
	msleep(20);

	ret = zx279051_fullmask_temperature_adjust(phydev);
	if (ret)
		return ret;

	an_adv = zx279051_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	if (an_adv < 0)
		return an_adv;
	ret = zx279051_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE,
				 an_adv | 0x0c00);
	if (ret)
		return ret;

	ret = zx279051_write_c22(phydev, MII_BMCR, BMCR_PDOWN);
	if (ret)
		return ret;
	ret = zx279051_write_c22(phydev, 21, 12);
	if (ret)
		return ret;
	msleep(20);
	ret = zx279051_write_c22(phydev, MII_BMCR, 0);
	if (ret)
		return ret;

	bmcr = mdiobus_read(phydev->mdio.bus, phydev->mdio.addr, MII_BMCR);
	if (bmcr < 0)
		return bmcr;
	ret = zx279051_write_c22(phydev, MII_BMCR, bmcr | BMCR_PDOWN);
	if (ret)
		return ret;

	phydev_dbg(phydev, "vendor post-reset initialization complete\n");
	return 0;
}

static int zx279051_probe(struct phy_device *phydev)
{
	struct zx279051_priv *priv;
	int id_first, id_second;
	int status;

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->host_interface = PHY_INTERFACE_MODE_NA;
	priv->host_speed_code = ~0U;
	phydev->priv = priv;

	id_first = zx279051_read_mmd(phydev, ZX279051_ID_DEVAD,
				     ZX279051_ID_REG);
	if (id_first < 0)
		return id_first;

	id_second = zx279051_read_mmd(phydev, ZX279051_ID_DEVAD,
				      ZX279051_ID_REG);
	if (id_second < 0)
		return id_second;

	if (id_first != ZX279051_PHY_ID || id_second != ZX279051_PHY_ID)
		return -ENODEV;

	status = mdiobus_read(phydev->mdio.bus, phydev->mdio.addr,
			      ZX279051_GE_STATUS);
	if (status < 0)
		return status;

	phydev_info(phydev, "identified at address %u, GE status %#x\n",
		    phydev->mdio.addr, status);

	return 0;
}

static int zx279051_config_init(struct phy_device *phydev)
{
	struct zx279051_priv *priv = phydev->priv;
	int ret;

	/*
	 * Rebuild the complete vendor post-reset state before PHYLIB clears
	 * BMCR_PDOWN in phy_resume().  The external reset loses both the line
	 * PHY calibration and the Uni-side profile.
	 */
	ret = zx279051_fullmask_init(phydev);
	if (ret)
		return ret;

	/*
	 * fullmask intentionally ends in BMCR_PDOWN.  Wake here rather than
	 * relying on a caller-specific phy_resume() so every phy_init_hw() path
	 * returns a usable PHY and reports a failed final MDIO write.
	 */
	ret = genphy_resume(phydev);
	if (ret)
		return ret;

	priv->host_interface = PHY_INTERFACE_MODE_NA;
	priv->host_speed_code = ~0U;
	__set_bit(PHY_INTERFACE_MODE_SGMII, phydev->possible_interfaces);
	__set_bit(PHY_INTERFACE_MODE_2500BASEX,
		  phydev->possible_interfaces);

	return 0;
}

static int zx279051_get_features(struct phy_device *phydev)
{
	/* GE status speed codes 0-6 already decode 10/100/1000/2500. */
	linkmode_set_bit(ETHTOOL_LINK_MODE_TP_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_Pause_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, phydev->supported);

	return 0;
}

/*
 * Vendor-Linux ZX279051 host-side management, recovered from zx279051.ko.
 * The PHY's Uni-side SerDes runs its own profile; phy_zxic_051_serdes_mode_set()
 * programs it through an indirect APB window on C45 devad 31:
 * address words at 31.18/31.17, data at 31.20/31.19, trigger at 31.21
 * (1 = write, 0 = read; read data at 31.23/31.22). Registers 0..192 form the
 * 49-word profile, register 84 gates the programming (1024 start, 1025
 * commit), and register 204 reports stability in bit 2.
 */
#define ZX279051_APB_DEVAD		MDIO_MMD_VEND2
#define ZX279051_APB_ADDR_HI		18
#define ZX279051_APB_ADDR_LO		17
#define ZX279051_APB_DATA_HI		20
#define ZX279051_APB_DATA_LO		19
#define ZX279051_APB_TRIGGER		21
#define ZX279051_APB_READ_HI		23
#define ZX279051_APB_READ_LO		22
#define ZX279051_APB_PROFILE_GATE	84
#define ZX279051_APB_PROFILE_START	1024
#define ZX279051_APB_PROFILE_COMMIT	1025
#define ZX279051_APB_STABLE_STATUS	204
#define ZX279051_APB_STABLE		BIT(2)
#define ZX279051_APB_XPCS_POWER		0x400c
#define ZX279051_APB_XPCS_POWER_ON	0x0440
#define ZX279051_APB_XPCS_POWER_OFF	0x0c40
#define ZX279051_STABLE_RETRIES		20
#define ZX279051_STABLE_DELAY_US	4295
#define ZX279051_REG_801E		0x801e
#define ZX279051_REG_801E_1G		180
#define ZX279051_REG_801E_2P5G		0
#define ZX279051_REG_80CC		0x80cc
#define ZX279051_REG_80CC_1G		127
#define ZX279051_REG_80CC_2P5G		70

struct zx279051_serdes_word {
	u32 addr;
	u32 value;
};

static int zx279051_apb_write(struct phy_device *phydev, u32 addr, u32 value)
{
	int ret;

	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_APB_ADDR_HI,
				addr >> 16);
	if (ret)
		return ret;
	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_APB_ADDR_LO,
				addr & 0xffff);
	if (ret)
		return ret;
	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_APB_DATA_HI,
				value >> 16);
	if (ret)
		return ret;
	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_APB_DATA_LO,
				value & 0xffff);
	if (ret)
		return ret;

	return mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_APB_TRIGGER, 1);
}

static int zx279051_apb_read(struct phy_device *phydev, u32 addr, u32 *value)
{
	int hi, lo;
	int ret;

	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_APB_ADDR_HI,
				addr >> 16);
	if (ret)
		return ret;
	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_APB_ADDR_LO,
				addr & 0xffff);
	if (ret)
		return ret;
	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_APB_TRIGGER, 0);
	if (ret)
		return ret;

	hi = mdiobus_c45_read(phydev->mdio.bus, phydev->mdio.addr,
			      ZX279051_APB_DEVAD, ZX279051_APB_READ_HI);
	if (hi < 0)
		return hi;
	lo = mdiobus_c45_read(phydev->mdio.bus, phydev->mdio.addr,
			      ZX279051_APB_DEVAD, ZX279051_APB_READ_LO);
	if (lo < 0)
		return lo;

	*value = (hi << 16) | lo;
	return 0;
}

/*
 * 051 mode_eth_1gbase_x_cfg: the PHY's internal Uni-side SerDes 1G profile,
 * identical to the SoC CPU129 1GBASE-X profile.
 */
static const struct zx279051_serdes_word zx279051_serdes_1g_profile[] = {
	{ 0x00, 0x00000004 }, { 0x04, 0x000040a0 }, { 0x08, 0x00008003 },
	{ 0x0c, 0x0c900000 }, { 0x10, 0x00000004 }, { 0x14, 0x00150000 },
	{ 0x18, 0x00000000 }, { 0x1c, 0x00000220 }, { 0x20, 0x80000000 },
	{ 0x24, 0x00050000 }, { 0x28, 0x00000010 }, { 0x2c, 0x00000421 },
	{ 0x30, 0x00000000 }, { 0x34, 0x00000000 }, { 0x38, 0x00000000 },
	{ 0x3c, 0x00000000 }, { 0x40, 0x05a8d100 }, { 0x44, 0x00000000 },
	{ 0x48, 0x0104db2a }, { 0x4c, 0x60002200 }, { 0x50, 0x00000603 },
	{ 0x54, 0x00000400 }, { 0x58, 0x00000080 }, { 0x5c, 0x00000000 },
	{ 0x60, 0x200554a8 }, { 0x64, 0x3de49092 }, { 0x68, 0x01000000 },
	{ 0x6c, 0xc0003700 }, { 0x70, 0xa9004000 }, { 0x74, 0x10108000 },
	{ 0x78, 0x000000da }, { 0x7c, 0xf20000c8 }, { 0x80, 0x10371037 },
	{ 0x84, 0x01000200 }, { 0x88, 0x00020001 }, { 0x8c, 0x10000000 },
	{ 0x90, 0x0000201c }, { 0x94, 0x00000000 }, { 0x98, 0x000000ff },
	{ 0x9c, 0x55555500 }, { 0xa0, 0x55555555 }, { 0xa4, 0x00555555 },
	{ 0xa8, 0x22000818 }, { 0xac, 0x0000201c }, { 0xb0, 0x0000000c },
	{ 0xb4, 0x01000000 }, { 0xb8, 0x00000080 }, { 0xbc, 0x00010000 },
	{ 0xc0, 0x00000000 },
};

/*
 * 051 mode_eth_2p5gbase_x_cfg: the PHY's 2.5G Uni-side SerDes profile; only
 * nine words differ from the 1G profile (offsets 8, 12, 20, 72, 76, 80,
 * 156, 160, and 164).
 */
static const struct zx279051_serdes_word zx279051_serdes_2p5g_profile[] = {
	{ 0x00, 0x00000004 }, { 0x04, 0x000040a0 }, { 0x08, 0x00008009 },
	{ 0x0c, 0x1f500000 }, { 0x10, 0x00000004 }, { 0x14, 0x00194000 },
	{ 0x18, 0x00000000 }, { 0x1c, 0x00000220 }, { 0x20, 0x80000000 },
	{ 0x24, 0x00050000 }, { 0x28, 0x00000010 }, { 0x2c, 0x00000421 },
	{ 0x30, 0x00000000 }, { 0x34, 0x00000000 }, { 0x38, 0x00000000 },
	{ 0x3c, 0x00000000 }, { 0x40, 0x05a8d100 }, { 0x44, 0x00000000 },
	{ 0x48, 0x0404db2a }, { 0x4c, 0x80002200 }, { 0x50, 0x00000604 },
	{ 0x54, 0x00000400 }, { 0x58, 0x00000080 }, { 0x5c, 0x00000000 },
	{ 0x60, 0x200554a8 }, { 0x64, 0x3de49092 }, { 0x68, 0x01000000 },
	{ 0x6c, 0xc0003700 }, { 0x70, 0xa9004000 }, { 0x74, 0x10108000 },
	{ 0x78, 0x000000da }, { 0x7c, 0xf20000c8 }, { 0x80, 0x10371037 },
	{ 0x84, 0x01000200 }, { 0x88, 0x00020001 }, { 0x8c, 0x10000000 },
	{ 0x90, 0x0000201c }, { 0x94, 0x00000000 }, { 0x98, 0x000000ff },
	{ 0x9c, 0x33333300 }, { 0xa0, 0x33333333 }, { 0xa4, 0x00333333 },
	{ 0xa8, 0x22000818 }, { 0xac, 0x0000201c }, { 0xb0, 0x0000000c },
	{ 0xb4, 0x01000000 }, { 0xb8, 0x00000080 }, { 0xbc, 0x00010000 },
	{ 0xc0, 0x00000000 },
};

static int zx279051_serdes_mode_set(struct phy_device *phydev,
				    const struct zx279051_serdes_word *profile,
				    size_t count)
{
	unsigned int i;
	int ret;

	ret = zx279051_apb_write(phydev, ZX279051_APB_PROFILE_GATE,
				 ZX279051_APB_PROFILE_START);
	if (ret)
		return ret;

	for (i = 0; i < count; i++) {
		ret = zx279051_apb_write(phydev, profile[i].addr,
					 profile[i].value);
		if (ret)
			return ret;
	}

	return zx279051_apb_write(phydev, ZX279051_APB_PROFILE_GATE,
				ZX279051_APB_PROFILE_COMMIT);
}

static int zx279051_serdes_wait_stable(struct phy_device *phydev)
{
	unsigned int i;
	u32 value;
	int ret;

	for (i = 0; i < ZX279051_STABLE_RETRIES; i++) {
		ret = zx279051_apb_read(phydev, ZX279051_APB_STABLE_STATUS,
					&value);
		if (ret)
			return ret;
		if (value & ZX279051_APB_STABLE)
			return 0;
		usleep_range(ZX279051_STABLE_DELAY_US,
			     ZX279051_STABLE_DELAY_US + 100);
	}

	return -ETIMEDOUT;
}

/*
 * phy_zxic_051_uniserdes_mode_check() maps GE status[9:7] to a Uni speed
 * byte (0=10, 1=100, 2=1000, 3=2500) and a SerDes mode. Modes 2/5/6 all
 * program mode_eth_1gbase_x_cfg; modes 3/4 program mode_eth_2p5gbase_x_cfg;
 * modes 0/1 write nothing. Live 10/100/1000 therefore share the 1G profile
 * (mode 6). There is no separate 10/100 Uni SerDes table in zx279051.ko.
 */
static int zx279051_host_bringup(struct phy_device *phydev)
{
	struct zx279051_priv *priv = phydev->priv;
	const struct zx279051_serdes_word *profile;
	size_t count;
	unsigned int speed_code;
	phy_interface_t interface;
	u16 reg_801e;
	u16 reg_80cc;
	u32 stable;
	int status;
	int ret;

	/*
	 * The line link is firmware-resolved before PHYLIB runs; derive the
	 * Uni-side mode from the GE status register exactly like
	 * phy_zxic_051_uniserdes_mode_check() (codes 4/5 = 1G, 6 = 2.5G).
	 */
	status = mdiobus_read(phydev->mdio.bus, phydev->mdio.addr,
			      ZX279051_GE_STATUS);
	if (status < 0)
		return status;
	if (!(status & ZX279051_GE_LINK))
		return 0;

	speed_code = FIELD_GET(ZX279051_GE_SPEED, status);
	if (speed_code > 6)
		return -EINVAL;

	if (speed_code == 6) {
		interface = PHY_INTERFACE_MODE_2500BASEX;
		profile = zx279051_serdes_2p5g_profile;
		count = ARRAY_SIZE(zx279051_serdes_2p5g_profile);
	} else {
		interface = PHY_INTERFACE_MODE_SGMII;
		profile = zx279051_serdes_1g_profile;
		count = ARRAY_SIZE(zx279051_serdes_1g_profile);
	}

	if (priv->host_interface == interface &&
	    priv->host_speed_code == speed_code)
		return 0;

	ret = zx279051_serdes_mode_set(phydev, profile, count);
	if (ret)
		return ret;

	ret = zx279051_serdes_wait_stable(phydev);
	if (ret) {
		/* phy_051_xpcs_power_ctrl(): power-cycle the internal XPCS. */
		ret = zx279051_apb_write(phydev, ZX279051_APB_XPCS_POWER,
					 ZX279051_APB_XPCS_POWER_OFF);
		if (ret)
			return ret;
		usleep_range(ZX279051_STABLE_DELAY_US,
			     ZX279051_STABLE_DELAY_US + 100);
		ret = zx279051_apb_write(phydev, ZX279051_APB_XPCS_POWER,
					 ZX279051_APB_XPCS_POWER_ON);
		if (ret)
			return ret;
		ret = zx279051_serdes_wait_stable(phydev);
		if (ret)
			return ret;
	}

	ret = zx279051_apb_read(phydev, ZX279051_APB_STABLE_STATUS, &stable);
	if (ret)
		return ret;
	phydev_info(phydev, "Uni SerDes stable, status %#x\n", stable);

	/* Vendor link-up flags differ between the 1G and 2.5G host modes. */
	if (speed_code == 6) {
		reg_801e = ZX279051_REG_801E_2P5G;
		reg_80cc = ZX279051_REG_80CC_2P5G;
	} else {
		reg_801e = ZX279051_REG_801E_1G;
		reg_80cc = ZX279051_REG_80CC_1G;
	}

	phydev_info(phydev, "host mode speed code %u, link flags %u/%u\n",
		    speed_code, reg_801e, reg_80cc);

	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				MDIO_MMD_PMAPMD, ZX279051_REG_801E,
				reg_801e);
	if (ret)
		return ret;

	ret = mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				ZX279051_APB_DEVAD, ZX279051_REG_80CC,
				reg_80cc);
	if (!ret) {
		priv->host_interface = interface;
		priv->host_speed_code = speed_code;
	}

	return ret;
}

/*
 * Vendor phy_zxic051_set_linkmode() programs line-side copper AN through
 * C45 MMD7 plus C22 MII_CTRL1000. 7.32 bit7 is 2.5GBASE-T advertise
 * (MDIO_AN_10GBT_CTRL_ADV2_5G). 7.16 uses the C22 advertisement bit
 * layout. C22 register 9 0x0300 advertises 1000 half+full. 7.0 0x3200 is
 * AN enable + restart; restart is self-clearing (live readback 0x3000).
 */
#define ZX279051_AN_ADV_VENDOR	0x1001
#define ZX279051_AN_CTRL_RESTART	0x3200
#define ZX279051_AN_10G_BASE	0x2001

static int zx279051_c45_an_write(struct phy_device *phydev, u16 reg, u16 val)
{
	return mdiobus_c45_write(phydev->mdio.bus, phydev->mdio.addr,
				 MDIO_MMD_AN, reg, val);
}

static int zx279051_c45_an_read(struct phy_device *phydev, u16 reg)
{
	return mdiobus_c45_read(phydev->mdio.bus, phydev->mdio.addr,
				MDIO_MMD_AN, reg);
}

static u16 zx279051_an_adv_from_linkmode(const unsigned long *adv)
{
	u16 reg = ZX279051_AN_ADV_VENDOR;

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT, adv))
		reg |= ADVERTISE_10HALF;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT, adv))
		reg |= ADVERTISE_10FULL;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT, adv))
		reg |= ADVERTISE_100HALF;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT, adv))
		reg |= ADVERTISE_100FULL;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT, adv))
		reg |= ADVERTISE_PAUSE_CAP;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, adv))
		reg |= ADVERTISE_PAUSE_ASYM;
	return reg;
}

static int zx279051_set_linkmode(struct phy_device *phydev)
{
	u16 an_10g = ZX279051_AN_10G_BASE;
	u16 ctrl1000;
	u16 adv;
	int ret;

	if (phydev->autoneg == AUTONEG_DISABLE)
		return -EOPNOTSUPP;

	adv = zx279051_an_adv_from_linkmode(phydev->advertising);
	ctrl1000 = linkmode_adv_to_mii_ctrl1000_t(phydev->advertising);
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
			      phydev->advertising))
		an_10g |= MDIO_AN_10GBT_CTRL_ADV2_5G;

	ret = zx279051_c45_an_write(phydev, MDIO_AN_10GBT_CTRL, an_10g);
	if (ret)
		return ret;
	ret = mdiobus_write(phydev->mdio.bus, phydev->mdio.addr,
			    MII_CTRL1000, ctrl1000);
	if (ret)
		return ret;
	ret = zx279051_c45_an_write(phydev, MDIO_AN_ADVERTISE, adv);
	if (ret)
		return ret;
	ret = zx279051_c45_an_write(phydev, MDIO_CTRL1,
				    ZX279051_AN_CTRL_RESTART);
	if (ret)
		return ret;

	return 0;
}

static int zx279051_config_aneg(struct phy_device *phydev)
{
	int ret;

	ret = zx279051_set_linkmode(phydev);
	if (ret)
		return ret;
	return zx279051_host_bringup(phydev);
}

static int zx279051_read_lpa(struct phy_device *phydev)
{
	int lpa, stat1000, stat;

	linkmode_zero(phydev->lp_advertising);
	lpa = zx279051_c45_an_read(phydev, MDIO_AN_LPA);
	if (lpa < 0)
		return lpa;
	mii_lpa_mod_linkmode_lpa_t(phydev->lp_advertising, lpa);

	stat1000 = mdiobus_read(phydev->mdio.bus, phydev->mdio.addr,
				MII_STAT1000);
	if (stat1000 < 0)
		return stat1000;
	mii_stat1000_mod_linkmode_lpa_t(phydev->lp_advertising, stat1000);

	stat = zx279051_c45_an_read(phydev, MDIO_AN_10GBT_STAT);
	if (stat < 0)
		return stat;
	if (stat & MDIO_AN_10GBT_STAT_LP2_5G)
		linkmode_set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
				 phydev->lp_advertising);

	return 0;
}

static int zx279051_read_status(struct phy_device *phydev)
{
	struct zx279051_priv *priv = phydev->priv;
	unsigned int speed_code;
	phy_interface_t interface;
	int status;
	int ret;

	status = mdiobus_read(phydev->mdio.bus, phydev->mdio.addr,
			      ZX279051_GE_STATUS);
	if (status < 0)
		return status;

	phydev->link = !!(status & ZX279051_GE_LINK);
	phydev->autoneg_complete = phydev->link;
	phydev->speed = SPEED_UNKNOWN;
	phydev->duplex = DUPLEX_UNKNOWN;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	linkmode_zero(phydev->lp_advertising);

	if (!phydev->link)
		return 0;

	speed_code = FIELD_GET(ZX279051_GE_SPEED, status);
	if (speed_code > 6)
		return -EINVAL;

	interface = speed_code == 6 ? PHY_INTERFACE_MODE_2500BASEX :
					    PHY_INTERFACE_MODE_SGMII;
	if (priv->host_interface != interface ||
	    priv->host_speed_code != speed_code) {
		ret = zx279051_host_bringup(phydev);
		if (ret)
			return ret;
	}
	phydev->interface = interface;

	switch (speed_code) {
	case 0:
	case 1:
		phydev->speed = SPEED_10;
		break;
	case 2:
	case 3:
		phydev->speed = SPEED_100;
		break;
	case 4:
	case 5:
		phydev->speed = SPEED_1000;
		break;
	case 6:
		phydev->speed = SPEED_2500;
		break;
	}

	phydev->duplex = speed_code & 1 || speed_code == 6 ?
			     DUPLEX_FULL : DUPLEX_HALF;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = zx279051_read_lpa(phydev);
		if (ret)
			return ret;
		phy_resolve_aneg_pause(phydev);
	}

	return 0;
}

static struct phy_driver zx279051_driver[] = {
	{
		PHY_ID_MATCH_EXACT(ZX279051_PHY_ID),
		.name		= "ZTE ZX279051",
		.probe		= zx279051_probe,
		.config_init	= zx279051_config_init,
		.get_features	= zx279051_get_features,
		.config_aneg	= zx279051_config_aneg,
		.read_status	= zx279051_read_status,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	},
};
module_phy_driver(zx279051_driver);

static const struct mdio_device_id __maybe_unused zx279051_tbl[] = {
	{ PHY_ID_MATCH_EXACT(ZX279051_PHY_ID) },
	{ }
};
MODULE_DEVICE_TABLE(mdio, zx279051_tbl);

MODULE_DESCRIPTION("ZTE ZX279051 Ethernet PHY driver");
MODULE_LICENSE("GPL");
