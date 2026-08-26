// SPDX-License-Identifier: GPL-2.0-only
/*
 * ZTE ZX279133 / RTL8372N DSA switch driver.
 *
 * The module owns the XMAC0/XPCS0/Uni-SerDes CPU path, the validated
 * RTL8372N ports4..7-to-CPU8 private transport image, and the board's
 * active-low switch reset line. Removal restores the SoC-side state and
 * holds the switch reset.
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/gpio/consumer.h>
#include <linux/if_vlan.h>
#include <linux/if_bridge.h>
#include <linux/iopoll.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <net/dsa.h>
#include <net/dsa_zx279133_rtl8372n.h>

#include "zx279133-lan.h"

#define ZX279133_XMAC0_BASE		0x140000
#define ZX279133_XMAC_TX_CTRL		0x0000
#define ZX279133_XMAC_RX_CTRL		0x0010
#define ZX279133_XMAC_FRAME_CFG		0x0020
#define ZX279133_XMAC_MODE_CFG		0x0280
#define ZX279133_XMAC_DUPLEX		0x0500
#define ZX279133_XMAC_MISC_CFG		0x3400
#define ZX279133_XMAC_HALF_DUPLEX	BIT(24)
#define ZX279133_XMAC_RESET_REG		0x2c0004
#define ZX279133_XMAC0_RESET		BIT(10)
#define ZX279133_XMAC_RESET_US		1718

#define ZX279133_XPCS_VR_MII_DIG_CTRL1	0x8000
#define ZX279133_XPCS_VR_MII_AN_CTRL	0x8001
#define ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1 0x8000
#define ZX279133_XPCS_VR_XS_PCS_KR_CTRL	0x8007
#define ZX279133_XPCS_USXG_EN		BIT(9)
#define ZX279133_XPCS_VSMMD1_EN	BIT(13)
#define ZX279133_XPCS_VR_RST		BIT(15)
#define ZX279133_XPCS_USXG_MODE_MASK	GENMASK(12, 10)
#define ZX279133_XPCS_AN_INTR_EN	BIT(0)
#define ZX279133_XPCS_AN_ENABLE		BIT(12)
/* Vendor mode-5 USXGMII image before the datapath tail disables AN. */
#define ZX279133_XPCS_USXGMII_AN_BMCR	0x3140
#define ZX279133_XPCS_BYPASS_REG	0x8005
#define ZX279133_XPCS_BYPASS_EN	BIT(4)

#define ZX279133_NPPT_XMAC_LINK		0x0084
#define ZX279133_NPPT_XPCS0_STATUS	0x0090
#define ZX279133_XMAC0_SOPC_READY	0x342a4
#define ZX279133_XMAC0_SOPC_SEND	0x342c0
#define ZX279133_XMAC_SOPC_DUPLEX	0x343f0
#define ZX279133_XMAC0_SOPC_DUPLEX_MASK BIT(4)

#define RTL8372N_SMI_CTRL		0x15
#define RTL8372N_SMI_BUSY		BIT(2)
#define RTL8372N_SMI_ADDR		0x16
#define RTL8372N_SMI_DATA_LOW		0x17
#define RTL8372N_SMI_DATA_HIGH		0x18
#define RTL8372N_SMI_READ_CMD		0x001b
#define RTL8372N_SMI_WRITE_CMD		0x0019
#define RTL8372N_SMI_POLL_MAX		1000
#define RTL8372N_CHIP_ID			0x0004
#define RTL8372N_PHY_PORT_SELECT	0x6438
#define RTL8372N_PHY_CTRL		0x643c
#define RTL8372N_PHY_DATA		0x6444
#define RTL8372N_PHY_BUSY		BIT(0)
#define RTL8372N_PHY_OP_STATUS		GENMASK(26, 24)
#define RTL8372N_PHY_WRITE_CMD		0x7
#define RTL8372N_SDS_CTRL		0x03f8
#define RTL8372N_SDS_READ_DATA		0x03fc
#define RTL8372N_SDS_WRITE_DATA	0x0400
#define RTL8372N_VERSION			0x000c
#define RTL8372N_PORT8_FORCE_ABILITY	0x6364
#define RTL8372N_ABILITY_STATUS		0x63e8
#define RTL8372N_MEDIA_STATUS		0x63ec
#define RTL8372N_SPEED_STATUS0		0x63f0
#define RTL8372N_SPEED_STATUS1		0x63f4
#define RTL8372N_DUPLEX_STATUS		0x63f8
#define RTL8372N_LINK_STATUS		0x63fc
#define RTL8372N_EXTERNAL_CPU_PORT	0x6724
#define RTL8372N_SDS_TOP_MODE		0x7b20
#define RTL8372N_SDS1_MODE_MASK	GENMASK(9, 5)
#define RTL8372N_SDS1_SUBMODE_MASK	GENMASK(20, 16)
#define RTL8372N_CFG_MAC8_8221B	BIT(22)
#define RTL8372N_SDS_MODE_10GUSXG	13
#define RTL8372N_PORT8_FORCE_10G	0x0227
#define RTL8372N_VLAN_CTRL		0x4e14
#define RTL8372N_MSTP0_STATE		0x5310
#define RTL8372N_VLAN_IGR_FILTER	0x4e18
#define RTL8372N_PORT_PVID_4_5		0x4e24
#define RTL8372N_PORT_PVID_6_7		0x4e28
#define RTL8372N_PORT_PVID_8_9		0x4e2c
#define RTL8372N_PORT_FID_ENABLE	0x4e38
#define RTL8372N_PORT_FID_4_7		0x4e3c
#define RTL8372N_PORT7_ACCEPT		0x4e10
#define RTL8372N_PORT_ISOLATION_BASE	0x50c0
#define RTL8372N_PORT7_ISOLATION	0x50dc
#define RTL8372N_PORT8_ISOLATION	0x50e0
#define RTL8372N_SVLAN_SERVICE_PORT	0x57c0
#define RTL8372N_TABLE_CTRL		0x5cac
#define RTL8372N_L2_CTRL		0x5cb0
#define RTL8372N_TABLE_WRITE_DATA0	0x5cb8
#define RTL8372N_TABLE_WRITE_DATA1	0x5cbc
#define RTL8372N_TABLE_WRITE_DATA2	0x5cc0
#define RTL8372N_TABLE_READ_DATA0	0x5ccc
#define RTL8372N_TABLE_READ_DATA1	0x5cd0
#define RTL8372N_TABLE_READ_DATA2	0x5cd4
#define RTL8372N_L2_READ_METHOD_MASK	GENMASK(17, 14)
#define RTL8372N_L2_READ_METHOD_NEXT_UC	(3 << 14)
#define RTL8372N_L2_LOOKUP_HIT		BIT(12)
#define RTL8372N_L2_MAX_ADDRESS		0x0fff
#define RTL8372N_L2_TABLE_COMMAND	(4 << 8)
#define RTL8372N_L2_READ_COMMAND	(RTL8372N_L2_TABLE_COMMAND | BIT(0))
#define RTL8372N_L2_WRITE_COMMAND	(RTL8372N_L2_TABLE_COMMAND | BIT(1) | BIT(0))
#define RTL8372N_MIB_CTRL		0x0f60
#define RTL8372N_MIB_DATA_LOW		0x0f64
#define RTL8372N_MIB_DATA_HIGH		0x0f68
#define RTL8372N_MIB_TX_GOOD_HIGH	92
#define RTL8372N_MIB_RX_GOOD_HIGH	94
#define RTL8372N_MIB_RX_ERROR		96
#define RTL8372N_MIB_TX_ERROR		97
#define RTL8372N_MIB_TX_GOOD_PHY_HIGH	98
#define RTL8372N_MIB_RX_GOOD_PHY_HIGH	100
#define RTL8372N_MIB_RX_ERROR_PHY	102
#define RTL8372N_MIB_TX_ERROR_PHY	103
#define RTL8372N_SVLAN_TPID		0x6044
#define RTL8372N_PORT_TAG_MODE_6_9	0x6738
#define RTL8372N_USER_PORT_MIN		4
#define RTL8372N_USER_PORT_MAX		7
#define RTL8372N_USER_PORT_MASK		GENMASK(7, 4)
#define RTL8372N_CPU_PORT		8
#define RTL8372N_TRANSPORT_VID_BASE	ZX279133_RTL8372N_TRANSPORT_VID_BASE
#define RTL8372N_VLAN_MBR_MASK		GENMASK(9, 0)
#define RTL8372N_VLAN_UNTAG_SHIFT	10
#define RTL8372N_VLAN_UNTAG_MASK	GENMASK(19, 10)
#define RTL8372N_VLAN_IVL		BIT(25)
#define RTL8372N_INIT_STATE		0x7f60
#define RTL8372N_RESET_ASSERT_MS	100
#define RTL8372N_RESET_DEASSERT_MS	100

struct zx279133_rtl8372n {
	struct zx279133_lan_service *service;
	struct mdio_device *xpcs_mdiodev;
	struct mdio_device *switch_mdiodev;
	struct gpio_desc *reset_gpio;
	struct phy *serdes;
	struct dsa_switch *ds;
	u16 saved_pcs_ctrl2;
	u16 saved_pcs_dig1;
	u16 saved_pcs_kr_ctrl;
	u16 saved_vend2_bmcr;
	u16 saved_vend2_an_ctrl;
	u16 saved_xpcs_bypass;
	u32 saved_xmac_tx;
	u32 saved_xmac_rx;
	u32 saved_xmac_frame;
	u32 saved_xmac_mode;
	u32 saved_xmac_duplex;
	u32 saved_xmac_misc;
	u32 saved_xmac_reset;
	u32 saved_sopc_duplex;
	u32 saved_sopc_send;
	bool parent_datapath_held;
	bool parent_datapath_ready;
	bool serdes_powered;
	bool xpcs_runtime_held;
	bool xpcs_configured;
	bool xmac_configured;
	bool datapath_enabled;
	bool switch_touched;
	bool switch_reset_asserted;
	bool switch_initialized;
	bool switch_cpu8_configured;
	bool vlan62_tx_active;
	bool dsa_registered;
	unsigned long vlan_filtering_mask;
	unsigned long vlan_unaware_vid1_mask;
	u16 bridge_pvid[9];
	bool bridge_pvid_valid[9];
};

static u32 zx279133_lan_nppt_read(struct zx279133_rtl8372n *priv, u32 offset)
{
	return zx279133_lan_service_nppt_read(priv->service, offset);
}

static void zx279133_lan_nppt_write(struct zx279133_rtl8372n *priv,
				    u32 offset, u32 value)
{
	zx279133_lan_service_nppt_write(priv->service, offset, value);
}

static void zx279133_lan_xmac_lock(struct zx279133_rtl8372n *priv)
{
	zx279133_lan_service_xmac_lock(priv->service);
}

static void zx279133_lan_xmac_unlock(struct zx279133_rtl8372n *priv)
{
	zx279133_lan_service_xmac_unlock(priv->service);
}

struct rtl8372n_sds_patch {
	u8 page;
	u8 reg;
	u16 value;
};

struct rtl8372n_sds_step {
	u8 page;
	u8 reg;
	u16 mask;
	u16 value;
	u16 delay_us;
};

/* RTL8372N revision-2 10.3125-Gbaud analog and MAC-side digital image. */
static const struct rtl8372n_sds_patch rtl8372n_sds_10g_chipb[] = {
	{ 0x21, 0x10, 0x4480 }, { 0x21, 0x13, 0x0400 },
	{ 0x21, 0x18, 0x6d02 }, { 0x21, 0x1b, 0x424e },
	{ 0x21, 0x1d, 0x0002 }, { 0x36, 0x1c, 0x1390 },
	{ 0x36, 0x14, 0x003f }, { 0x36, 0x10, 0x0200 },
	{ 0x2e, 0x04, 0x0080 }, { 0x2e, 0x06, 0x0408 },
	{ 0x2e, 0x07, 0x020d }, { 0x2e, 0x09, 0x0601 },
	{ 0x2e, 0x0b, 0x222c }, { 0x2e, 0x0c, 0xa217 },
	{ 0x2e, 0x0d, 0xfe40 }, { 0x2e, 0x15, 0xf5c1 },
	{ 0x2e, 0x16, 0x0443 }, { 0x2e, 0x1d, 0xabb0 },
};

static const struct rtl8372n_sds_patch rtl8372n_sds_mac_digital[] = {
	{ 0x06, 0x12, 0x5078 }, { 0x07, 0x06, 0x9401 },
	{ 0x07, 0x08, 0x9401 }, { 0x07, 0x0a, 0x9401 },
	{ 0x07, 0x0c, 0x9401 }, { 0x1f, 0x0b, 0x0003 },
	{ 0x06, 0x03, 0xc45c }, { 0x06, 0x1f, 0x2100 },
};

static const struct rtl8372n_sds_step rtl8372n_sds_mode_pre[] = {
	{ 0x20, 0, 0x0030, 0x0030, 10 },
	{ 0x20, 0, 0x0030, 0x0010, 100 },
	{ 0x20, 0, 0x00c0, 0x0040, 10 },
	{ 0x20, 0, 0x00c0, 0x00c0, 100 },
	{ 0x20, 0, 0x0c00, 0x0c00, 10 },
	{ 0x20, 0, 0x0c00, 0x0400, 100 },
};

static const struct rtl8372n_sds_step rtl8372n_sds_mode_reset[] = {
	{ 0x20, 0, 0x0030, 0x0030, 10 },
	{ 0x20, 0, 0x0030, 0x0010, 100 },
	{ 0x20, 0, 0x00c0, 0x0040, 10 },
	{ 0x20, 0, 0x00c0, 0x00c0, 100 },
	{ 0x20, 0, 0x0c00, 0x0c00, 10 },
	{ 0x20, 0, 0x0c00, 0x0400, 10 },
	{ 0x20, 0, 0x0c00, 0x0400, 10 },
	{ 0x20, 0, 0x0c00, 0x0c00, 100 },
	{ 0x20, 0, 0x0c00, 0x0000, 10 },
	{ 0x20, 0, 0x00c0, 0x00c0, 10 },
	{ 0x20, 0, 0x00c0, 0x0040, 100 },
	{ 0x20, 0, 0x00c0, 0x0000, 10 },
	{ 0x20, 0, 0x0030, 0x0010, 10 },
	{ 0x20, 0, 0x0030, 0x0030, 100 },
	{ 0x20, 0, 0x0030, 0x0000, 100 },
	{ 0x1f, 0, 0xffff, 0x000b, 100 },
	{ 0x1f, 0, 0xffff, 0x0000, 100 },
};

static void rtl8372n_reset_assert(struct zx279133_rtl8372n *priv)
{
	if (!priv->reset_gpio || priv->switch_reset_asserted)
		return;

	gpiod_set_value_cansleep(priv->reset_gpio, 1);
	priv->switch_reset_asserted = true;
	priv->switch_touched = false;
	priv->switch_initialized = false;
	priv->switch_cpu8_configured = false;
}

static void rtl8372n_hw_reset(struct device *dev,
			      struct zx279133_rtl8372n *priv)
{
	rtl8372n_reset_assert(priv);
	msleep(RTL8372N_RESET_ASSERT_MS);
	gpiod_set_value_cansleep(priv->reset_gpio, 0);
	priv->switch_reset_asserted = false;
	msleep(RTL8372N_RESET_DEASSERT_MS);
	dev_info(dev, "RTL8372N hardware reset completed\n");
}

static int rtl8372n_wait_ready(struct mdio_device *mdiodev)
{
	unsigned int i;
	int value;

	for (i = 0; i < RTL8372N_SMI_POLL_MAX; i++) {
		value = __mdiodev_read(mdiodev, RTL8372N_SMI_CTRL);
		if (value < 0)
			return value;
		if (!(value & RTL8372N_SMI_BUSY))
			return 0;
		if (i + 1 < RTL8372N_SMI_POLL_MAX)
			usleep_range(100, 200);
	}

	return -ETIMEDOUT;
}

static int rtl8372n_read_reg(struct mdio_device *mdiodev, u16 reg, u32 *value)
{
	int low;
	int high;
	int ret;

	ret = rtl8372n_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = __mdiodev_write(mdiodev, RTL8372N_SMI_ADDR, reg);
	if (ret)
		return ret;
	ret = __mdiodev_write(mdiodev, RTL8372N_SMI_CTRL,
			      RTL8372N_SMI_READ_CMD);
	if (ret)
		return ret;
	ret = rtl8372n_wait_ready(mdiodev);
	if (ret)
		return ret;
	low = __mdiodev_read(mdiodev, RTL8372N_SMI_DATA_LOW);
	if (low < 0)
		return low;
	high = __mdiodev_read(mdiodev, RTL8372N_SMI_DATA_HIGH);
	if (high < 0)
		return high;
	*value = (u32)low | (u32)high << 16;

	return 0;
}

static int rtl8372n_write_reg(struct mdio_device *mdiodev, u16 reg, u32 value)
{
	int ret;

	ret = rtl8372n_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = __mdiodev_write(mdiodev, RTL8372N_SMI_ADDR, reg);
	if (ret)
		return ret;
	ret = __mdiodev_write(mdiodev, RTL8372N_SMI_DATA_LOW,
			      value & 0xffff);
	if (ret)
		return ret;
	ret = __mdiodev_write(mdiodev, RTL8372N_SMI_DATA_HIGH, value >> 16);
	if (ret)
		return ret;
	ret = __mdiodev_write(mdiodev, RTL8372N_SMI_CTRL,
			      RTL8372N_SMI_WRITE_CMD);
	if (ret)
		return ret;

	return rtl8372n_wait_ready(mdiodev);
}

static int rtl8372n_modify_reg(struct mdio_device *mdiodev, u16 reg,
			       u32 mask, u32 set)
{
	u32 value;
	int ret;

	ret = rtl8372n_read_reg(mdiodev, reg, &value);
	if (ret)
		return ret;

	return rtl8372n_write_reg(mdiodev, reg,
				  (value & ~mask) | (set & mask));
}

static int rtl8372n_table_wait_ready(struct mdio_device *mdiodev)
{
	unsigned int i;
	u32 value;
	int ret;

	for (i = 0; i < RTL8372N_SMI_POLL_MAX; i++) {
		ret = rtl8372n_read_reg(mdiodev, RTL8372N_TABLE_CTRL, &value);
		if (ret)
			return ret;
		if (!(value & BIT(0)))
			return 0;
		if (i + 1 < RTL8372N_SMI_POLL_MAX)
			fsleep(10);
	}

	return -ETIMEDOUT;
}

static int rtl8372n_vlan_write(struct mdio_device *mdiodev, u16 vid,
			       u32 value)
{
	int ret;

	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_WRITE_DATA0, value);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_CTRL,
				 (vid << 16) | (3 << 8) | BIT(1) | BIT(0));
	if (ret)
		return ret;

	return rtl8372n_table_wait_ready(mdiodev);
}

static int rtl8372n_vlan_read(struct mdio_device *mdiodev, u16 vid,
			      u32 *value)
{
	int ret;

	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_CTRL,
				 (vid << 16) | (3 << 8) | BIT(0));
	if (ret)
		return ret;
	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;

	return rtl8372n_read_reg(mdiodev, RTL8372N_TABLE_READ_DATA0, value);
}

static bool rtl8372n_is_transport_vid(u16 vid)
{
	return (vid >= RTL8372N_TRANSPORT_VID_BASE + RTL8372N_USER_PORT_MIN &&
		vid <= RTL8372N_TRANSPORT_VID_BASE + RTL8372N_USER_PORT_MAX) ||
	       vid == ZX279133_RTL8372N_LAN1_TX_VID;
}

static int rtl8372n_port_pvid_field(int port, u16 *reg, u32 *mask,
				    unsigned int *shift)
{
	switch (port) {
	case 4:
		*reg = RTL8372N_PORT_PVID_4_5;
		*shift = 0;
		break;
	case 5:
		*reg = RTL8372N_PORT_PVID_4_5;
		*shift = 12;
		break;
	case 6:
		*reg = RTL8372N_PORT_PVID_6_7;
		*shift = 0;
		break;
	case 7:
		*reg = RTL8372N_PORT_PVID_6_7;
		*shift = 12;
		break;
	default:
		return -EINVAL;
	}
	*mask = GENMASK(*shift + 11, *shift);
	return 0;
}

static int rtl8372n_port_pvid_write(struct mdio_device *mdiodev, int port,
				    u16 pvid)
{
	unsigned int shift;
	u32 mask;
	u32 value;
	u16 reg;
	int ret;

	ret = rtl8372n_port_pvid_field(port, &reg, &mask, &shift);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, reg, mask, (u32)pvid << shift);
	if (ret)
		return ret;
	ret = rtl8372n_read_reg(mdiodev, reg, &value);
	if (ret)
		return ret;

	return ((value & mask) >> shift) == pvid ? 0 : -EIO;
}

static int rtl8372n_port_pvid_read(struct mdio_device *mdiodev, int port,
				   u16 *pvid)
{
	unsigned int shift;
	u32 mask;
	u32 value;
	u16 reg;
	int ret;

	ret = rtl8372n_port_pvid_field(port, &reg, &mask, &shift);
	if (ret)
		return ret;
	ret = rtl8372n_read_reg(mdiodev, reg, &value);
	if (ret)
		return ret;
	*pvid = (value & mask) >> shift;
	return 0;
}

static void rtl8372n_l2_encode_key(const unsigned char *addr, u16 fid,
				   bool ivl, u32 words[3])
{
	words[0] = addr[5] | ((u32)addr[4] << 8) |
		   ((u32)addr[3] << 16) | ((u32)addr[2] << 24);
	words[1] = addr[1] | ((u32)addr[0] << 8) |
		   ((u32)(fid & 0xfff) << 16) |
		   (ivl ? BIT(29) : 0);
	words[2] = 0;
}

static int rtl8372n_l2_write_words(struct mdio_device *mdiodev,
				   const u32 words[3])
{
	u32 ctrl;
	int ret;

	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_WRITE_DATA0,
				 words[0]);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_WRITE_DATA1,
				 words[1]);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_WRITE_DATA2,
				 words[2]);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_CTRL,
				 RTL8372N_L2_WRITE_COMMAND);
	if (ret)
		return ret;
	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_L2_CTRL, &ctrl);
	if (ret)
		return ret;

	return ctrl & RTL8372N_L2_LOOKUP_HIT ? 0 : -ENOSPC;
}

static int rtl8372n_l2_lookup(struct mdio_device *mdiodev,
			      const unsigned char *addr, u16 fid, bool ivl,
			       u32 words[3])
{
	u32 key[3];
	u32 ctrl;
	int ret;

	rtl8372n_l2_encode_key(addr, fid, ivl, key);
	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_L2_CTRL,
				  RTL8372N_L2_READ_METHOD_MASK, 0);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_WRITE_DATA0, key[0]);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_WRITE_DATA1, key[1]);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_WRITE_DATA2, key[2]);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_CTRL,
				 RTL8372N_L2_READ_COMMAND);
	if (ret)
		return ret;
	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_L2_CTRL, &ctrl);
	if (ret)
		return ret;
	if (!(ctrl & RTL8372N_L2_LOOKUP_HIT))
		return -ENOENT;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_TABLE_READ_DATA0,
				&words[0]);
	if (ret)
		return ret;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_TABLE_READ_DATA1,
				&words[1]);
	if (ret)
		return ret;

	return rtl8372n_read_reg(mdiodev, RTL8372N_TABLE_READ_DATA2,
				 &words[2]);
}

static int rtl8372n_l2_next_uc(struct mdio_device *mdiodev, u16 start,
			       u16 *address, u32 words[3])
{
	u32 ctrl;
	int ret;

	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_L2_CTRL,
				  RTL8372N_L2_READ_METHOD_MASK,
				  RTL8372N_L2_READ_METHOD_NEXT_UC);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_TABLE_CTRL,
				 RTL8372N_L2_READ_COMMAND | ((u32)start << 16));
	if (ret)
		return ret;
	ret = rtl8372n_table_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_L2_CTRL, &ctrl);
	if (ret)
		return ret;
	if (!(ctrl & RTL8372N_L2_LOOKUP_HIT))
		return -ENOENT;

	*address = ctrl & RTL8372N_L2_MAX_ADDRESS;
	if (*address < start)
		return -EIO;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_TABLE_READ_DATA0,
				&words[0]);
	if (ret)
		return ret;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_TABLE_READ_DATA1,
				&words[1]);
	if (ret)
		return ret;

	return rtl8372n_read_reg(mdiodev, RTL8372N_TABLE_READ_DATA2,
				 &words[2]);
}

static int rtl8372n_mib_read(struct mdio_device *mdiodev, unsigned int port,
			     unsigned int counter, u64 *value)
{
	unsigned int i;
	u32 low, high, ctrl;
	int ret;

	ctrl = BIT(0) | ((port & 0xf) << 1) |
	       (((counter / 2) & 0x3f) << 5);
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_MIB_CTRL, ctrl);
	if (ret)
		return ret;
	for (i = 0; i < 100; i++) {
		ret = rtl8372n_read_reg(mdiodev, RTL8372N_MIB_CTRL, &ctrl);
		if (ret)
			return ret;
		if (!(ctrl & BIT(0)))
			break;
		fsleep(10);
	}
	if (i == 100)
		return -ETIMEDOUT;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_MIB_DATA_LOW, &low);
	if (ret)
		return ret;
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_MIB_DATA_HIGH, &high);
	if (ret)
		return ret;

	*value = ((u64)low << 32) | high;
	return 0;
}

static int rtl8372n_phy_wait_ready(struct mdio_device *mdiodev)
{
	unsigned int i;
	u32 value;
	int ret;

	for (i = 0; i < RTL8372N_SMI_POLL_MAX; i++) {
		ret = rtl8372n_read_reg(mdiodev, RTL8372N_PHY_CTRL, &value);
		if (ret)
			return ret;
		if (!(value & RTL8372N_PHY_BUSY) &&
		    !(value & RTL8372N_PHY_OP_STATUS))
			return 0;
		if (i + 1 < RTL8372N_SMI_POLL_MAX)
			fsleep(10);
	}

	return -ETIMEDOUT;
}

static int rtl8372n_phy_write(struct mdio_device *mdiodev, u32 port_mask,
			      u16 page, u16 reg, u16 value)
{
	u32 command;
	int ret;

	ret = rtl8372n_write_reg(mdiodev, RTL8372N_PHY_PORT_SELECT, port_mask);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PHY_DATA,
				  GENMASK(15, 0), value);
	if (ret)
		return ret;
	command = (u32)page << 19 | (u32)reg << 3 | RTL8372N_PHY_WRITE_CMD;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_PHY_CTRL, command);
	if (ret)
		return ret;

	return rtl8372n_phy_wait_ready(mdiodev);
}

static int rtl8372n_sds_wait_ready(struct mdio_device *mdiodev)
{
	unsigned int i;
	u32 value;
	int ret;

	for (i = 0; i < RTL8372N_SMI_POLL_MAX; i++) {
		ret = rtl8372n_read_reg(mdiodev, RTL8372N_SDS_CTRL, &value);
		if (ret)
			return ret;
		if (!(value & BIT(15)))
			return 0;
		if (i + 1 < RTL8372N_SMI_POLL_MAX)
			fsleep(10);
	}

	return -ETIMEDOUT;
}

static int rtl8372n_sds_read(struct mdio_device *mdiodev, unsigned int sds,
			     unsigned int page, unsigned int reg, u32 *value)
{
	int ret;

	ret = rtl8372n_sds_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL, BIT(0),
				  sds ? BIT(0) : 0);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL, GENMASK(6, 1),
				  page << 1);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL,
				  GENMASK(11, 7), reg << 7);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL, BIT(14), 0);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL,
				  BIT(15), BIT(15));
	if (ret)
		return ret;
	ret = rtl8372n_sds_wait_ready(mdiodev);
	if (ret)
		return ret;

	return rtl8372n_read_reg(mdiodev, RTL8372N_SDS_READ_DATA, value);
}

static int rtl8372n_sds_write(struct mdio_device *mdiodev, unsigned int sds,
			      unsigned int page, unsigned int reg, u32 value)
{
	int ret;

	ret = rtl8372n_sds_wait_ready(mdiodev);
	if (ret)
		return ret;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_SDS_WRITE_DATA, value);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL, BIT(0),
				  sds ? BIT(0) : 0);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL, GENMASK(6, 1),
				  page << 1);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL,
				  GENMASK(11, 7), reg << 7);
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL,
				  BIT(14), BIT(14));
	if (ret)
		return ret;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_CTRL,
				  BIT(15), BIT(15));
	if (ret)
		return ret;

	return rtl8372n_sds_wait_ready(mdiodev);
}

static int rtl8372n_sds_modify(struct mdio_device *mdiodev, unsigned int sds,
			       unsigned int page, unsigned int reg,
			       u32 mask, u32 set)
{
	u32 value;
	int ret;

	ret = rtl8372n_sds_read(mdiodev, sds, page, reg, &value);
	if (ret)
		return ret;

	return rtl8372n_sds_write(mdiodev, sds, page, reg,
				   (value & ~mask) | (set & mask));
}

static int rtl8372n_fw_reset_flow(struct mdio_device *mdiodev,
				  unsigned int sds)
{
	u32 value;
	int ret;

	ret = rtl8372n_sds_read(mdiodev, sds, 0x20, 0, &value);
	if (ret || FIELD_GET(GENMASK(5, 4), value) == 1)
		return ret;
	ret = rtl8372n_sds_modify(mdiodev, sds, 0x21, 0, BIT(2), BIT(2));
	if (ret)
		return ret;
	ret = rtl8372n_sds_modify(mdiodev, sds, 0x36, 5,
				  GENMASK(14, 11), 8 << 11);
	if (ret)
		return ret;
	ret = rtl8372n_sds_write(mdiodev, sds, 0x1f, 2, 0x1f);
	if (ret)
		return ret;
	ret = rtl8372n_sds_read(mdiodev, sds, 0x1f, 0x15, &value);
	if (ret)
		return ret;
	if (!(value & BIT(6)) && (value & BIT(7)))
		return 0;
	ret = rtl8372n_sds_read(mdiodev, sds, 5, 0, &value);
	if (ret)
		return ret;
	if (value & BIT(0)) {
		ret = rtl8372n_sds_read(mdiodev, sds, 5, 0, &value);
		if (ret)
			return ret;
		if (!(value & BIT(1)) && (value & BIT(12)))
			return 0;
	}

	ret = rtl8372n_sds_modify(mdiodev, sds, 0x20, 0,
				  GENMASK(5, 4), 3 << 4);
	if (ret)
		return ret;
	ret = rtl8372n_sds_modify(mdiodev, sds, 0x20, 0,
				  GENMASK(5, 4), 1 << 4);
	if (ret)
		return ret;
	ret = rtl8372n_sds_modify(mdiodev, sds, 0x20, 0,
				  GENMASK(5, 4), 3 << 4);
	if (ret)
		return ret;
	ret = rtl8372n_sds_modify(mdiodev, sds, 0x20, 0,
				  GENMASK(5, 4), 0);
	if (ret)
		return ret;

	if (!(value & BIT(0)))
		return rtl8372n_sds_read(mdiodev, sds, 5, 0, &value);

	return 0;
}

static int rtl8372n_sds_apply_patches(struct mdio_device *mdiodev,
				      const struct rtl8372n_sds_patch *patches,
				       size_t count)
{
	size_t i;
	int ret;

	for (i = 0; i < count; i++) {
		ret = rtl8372n_sds_write(mdiodev, 1, patches[i].page,
					 patches[i].reg, patches[i].value);
		if (ret)
			return ret;
	}

	return 0;
}

static int rtl8372n_sds_run_steps(struct mdio_device *mdiodev,
				  const struct rtl8372n_sds_step *steps,
				   size_t count)
{
	size_t i;
	int ret;

	for (i = 0; i < count; i++) {
		ret = rtl8372n_sds_modify(mdiodev, 1, steps[i].page,
					  steps[i].reg, steps[i].mask,
					steps[i].value);
		if (ret)
			return ret;
		if (steps[i].delay_us)
			fsleep(steps[i].delay_us);
	}

	return 0;
}

static int rtl8372n_cpu8_link_init(struct device *dev,
				   struct zx279133_rtl8372n *priv)
{
	struct mdio_device *mdiodev = priv->switch_mdiodev;
	u32 value;
	int ret;

	mutex_lock(&mdiodev->bus->mdio_lock);
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_CHIP_ID, &value);
	if (ret)
		goto out_unlock;
	if ((value >> 8) != 0x837270) {
		ret = -ENODEV;
		goto out_unlock;
	}
	ret = rtl8372n_read_reg(mdiodev, 0x632c, &value);
	if (ret)
		goto out_unlock;
	if ((value & 0x1ff000) != 0x1f8000) {
		dev_err(dev, "RTL8372N core is not initialized: reg632c=%#x\n",
			value);
		ret = -EAGAIN;
		goto out_unlock;
	}

	priv->switch_touched = true;

	/* Vendor board contract forces port 8 to 10G/full/link before SDS1. */
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_PORT8_FORCE_ABILITY,
				 RTL8372N_PORT8_FORCE_10G);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_TOP_MODE,
				  RTL8372N_CFG_MAC8_8221B, 0);
	if (ret)
		goto out_unlock;
	usleep_range(1000, 1100);
	ret = rtl8372n_sds_run_steps(mdiodev, rtl8372n_sds_mode_pre,
				     ARRAY_SIZE(rtl8372n_sds_mode_pre));
	if (ret)
		goto out_unlock;

	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SDS_TOP_MODE,
				  RTL8372N_SDS1_MODE_MASK |
				 RTL8372N_SDS1_SUBMODE_MASK,
				 RTL8372N_SDS_MODE_10GUSXG << 5);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_apply_patches(mdiodev, rtl8372n_sds_10g_chipb,
					 ARRAY_SIZE(rtl8372n_sds_10g_chipb));
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_apply_patches(mdiodev,
					 rtl8372n_sds_mac_digital,
					 ARRAY_SIZE(rtl8372n_sds_mac_digital));
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_modify(mdiodev, 1, 0x07, 0x11, 0x000f, 0x000f);
	if (ret)
		goto out_unlock;
	usleep_range(1000, 1100);
	ret = rtl8372n_sds_run_steps(mdiodev, rtl8372n_sds_mode_reset,
				     ARRAY_SIZE(rtl8372n_sds_mode_reset));
	if (ret)
		goto out_unlock;
	fsleep(50);
	ret = rtl8372n_fw_reset_flow(mdiodev, 1);
	if (ret)
		goto out_unlock;
	fsleep(50);

	/* SR1010-specific polarity and 64b/66b settings after mode selection. */
	ret = rtl8372n_sds_modify(mdiodev, 1, 0, 0, 0x0200, 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_modify(mdiodev, 1, 6, 2, 0x2000, 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_modify(mdiodev, 1, 0, 0, 0x0100, 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_modify(mdiodev, 1, 6, 2, 0x4000, 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_write(mdiodev, 1, 0x20, 0, 0x0010);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_write(mdiodev, 1, 0x20, 0, 0x0000);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_modify(mdiodev, 1, 0x07, 0x11, 0x000f, 0);
	if (ret)
		goto out_unlock;

	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_EXTERNAL_CPU_PORT,
				  GENMASK(3, 0), 8);
	if (ret)
		goto out_unlock;
	/* zte_priv_init() marks external CPU port 8 as an SVLAN service port. */
	ret = rtl8372n_modify_reg(mdiodev, 0x57c0, BIT(8), BIT(8));
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_write(mdiodev, 1, 0x20, 0, 0x0010);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_write(mdiodev, 1, 0x20, 0, 0x0000);
	if (ret)
		goto out_unlock;

	priv->switch_cpu8_configured = true;

out_unlock:
	mutex_unlock(&mdiodev->bus->mdio_lock);
	if (ret) {
		dev_err(dev,
			"RTL8372N CPU-port-8 setup failed; hardware reset will be asserted: %d\n",
			ret);
		return ret;
	}

	dev_info(dev,
		 "RTL8372N SDS1 mode 13 and forced external CPU port 8 configured\n");
	return 0;
}

static int rtl8372n_port7_vlan_init(struct device *dev,
				    struct zx279133_rtl8372n *priv)
{
	struct mdio_device *mdiodev = priv->switch_mdiodev;
	u32 value;
	int ret;

	mutex_lock(&mdiodev->bus->mdio_lock);
	priv->switch_touched = true;

	/* Strip RX VID62 toward both Port7 and CPU8 so the active LAN1 fast path
	 * reaches PPU as an ordinary Ethernet frame. Use a separate TX-only VID
	 * because RTL8372N cannot ingress a tagged CPU8 frame through an entry
	 * that also marks CPU8 untagged.
	 */
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_SVLAN_TPID, 0x8100);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_SVLAN_SERVICE_PORT,
				  BIT(8), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT7_ACCEPT,
				  GENMASK(17, 14), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_write(mdiodev, 62, 0x00060180);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_write(mdiodev,
				 ZX279133_RTL8372N_LAN1_TX_VID, 0x00020180);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_PORT7_ISOLATION, 0x180);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_write_reg(mdiodev, RTL8372N_PORT8_ISOLATION, 0x1ff);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_FID_ENABLE,
				  BIT(7), BIT(7));
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_FID_4_7,
				  GENMASK(31, 28), 1 << 28);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_PVID_6_7,
				  GENMASK(23, 12), 62 << 12);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_PVID_8_9,
				  GENMASK(11, 0), 1);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_TAG_MODE_6_9,
				  GENMASK(17, 14), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_VLAN_CTRL, BIT(2), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_VLAN_IGR_FILTER,
				  BIT(7) | BIT(8), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_read(mdiodev, 62, &value);
	if (ret)
		goto out_unlock;
	if (value != 0x00060180) {
		dev_err(dev, "RTL8372N VLAN62 readback mismatch: %#x\n", value);
		ret = -EIO;
		goto out_unlock;
	}
	ret = rtl8372n_vlan_read(mdiodev,
				ZX279133_RTL8372N_LAN1_TX_VID, &value);
	if (ret)
		goto out_unlock;
	if (value != 0x00020180) {
		dev_err(dev, "RTL8372N VLAN63 readback mismatch: %#x\n", value);
		ret = -EIO;
		goto out_unlock;
	}

	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT7_ACCEPT,
				  GENMASK(9, 8), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_write(mdiodev, 59, 0x00004110);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_write_reg(mdiodev, 0x50d0, 0x110);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_FID_ENABLE,
				  BIT(4), BIT(4));
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_FID_4_7,
				  GENMASK(19, 16), 1 << 16);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, 0x4e24,
				  GENMASK(11, 0), 59);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_TAG_MODE_6_9,
				  GENMASK(9, 8), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_VLAN_IGR_FILTER,
				  BIT(4), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_read(mdiodev, 59, &value);
	if (ret)
		goto out_unlock;
	if (value != 0x00004110) {
		dev_err(dev, "RTL8372N VLAN59 readback mismatch: %#x\n",
			value);
		ret = -EIO;
		goto out_unlock;
	}

	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT7_ACCEPT,
				  GENMASK(11, 10), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_write(mdiodev, 60, 0x00008120);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_write_reg(mdiodev, 0x50d4, 0x120);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_FID_ENABLE,
				  BIT(5), BIT(5));
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_FID_4_7,
				  GENMASK(23, 20), 1 << 20);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, 0x4e24,
				  GENMASK(23, 12), 60 << 12);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_TAG_MODE_6_9,
				  GENMASK(11, 10), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_VLAN_IGR_FILTER,
				  BIT(5), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_read(mdiodev, 60, &value);
	if (ret)
		goto out_unlock;
	if (value != 0x00008120) {
		dev_err(dev, "RTL8372N VLAN60 readback mismatch: %#x\n",
			value);
		ret = -EIO;
		goto out_unlock;
	}

	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT7_ACCEPT,
				  GENMASK(13, 12), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_write(mdiodev, 61, 0x00010140);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_write_reg(mdiodev, 0x50d8, 0x140);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_FID_ENABLE,
				  BIT(6), BIT(6));
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_FID_4_7,
				  GENMASK(27, 24), 1 << 24);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_PVID_6_7,
				  GENMASK(11, 0), 61);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_PORT_TAG_MODE_6_9,
				  GENMASK(13, 12), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, RTL8372N_VLAN_IGR_FILTER,
				  BIT(6), 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_vlan_read(mdiodev, 61, &value);
	if (ret)
		goto out_unlock;
	if (value != 0x00010140) {
		dev_err(dev, "RTL8372N VLAN61 readback mismatch: %#x\n",
			value);
		ret = -EIO;
		goto out_unlock;
	}

 out_unlock:
	mutex_unlock(&mdiodev->bus->mdio_lock);
	if (ret) {
		dev_err(dev,
			"RTL8372N LAN transport VLAN setup failed; hardware reset will be asserted: %d\n",
			ret);
		return ret;
	}

	dev_info(dev,
		 "RTL8372N C-VLAN59..62 RX and LAN1 VID63 TX transport configured\n");
	return 0;
}

static int rtl8372n_minimal_core_init(struct device *dev,
				      struct zx279133_rtl8372n *priv)
{
	struct mdio_device *mdiodev = priv->switch_mdiodev;
	u32 value;
	u32 core_632c;
	u32 core_6330;
	u32 core_6334;
	u32 core_6454;
	unsigned int port;
	unsigned int reg;
	bool phy_disabled = false;
	int recovery_ret;
	int ret;

	mutex_lock(&mdiodev->bus->mdio_lock);
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_CHIP_ID, &value);
	if (ret)
		goto out_unlock;
	if ((value >> 8) != 0x837270) {
		dev_err(dev, "unsupported switch chip ID %#x\n", value);
		ret = -ENODEV;
		goto out_unlock;
	}
	ret = rtl8372n_read_reg(mdiodev, RTL8372N_INIT_STATE, &value);
	if (ret)
		goto out_unlock;
	if ((value & GENMASK(1, 0)) != 2) {
		dev_err(dev, "RTL8372N unsupported init state %#x\n", value);
		ret = -ENODEV;
		goto out_unlock;
	}

	ret = rtl8372n_read_reg(mdiodev, 0x632c, &core_632c);
	if (!ret)
		ret = rtl8372n_read_reg(mdiodev, 0x6330, &core_6330);
	if (!ret)
		ret = rtl8372n_read_reg(mdiodev, 0x6334, &core_6334);
	if (!ret)
		ret = rtl8372n_read_reg(mdiodev, 0x6454, &core_6454);
	if (ret)
		goto out_unlock;
	if (core_632c == 0x001f8540 && core_6330 == 0x00005515 &&
	    core_6334 == 0x000000f0 && core_6454 == 0x00007000) {
		mutex_unlock(&mdiodev->bus->mdio_lock);
		priv->switch_initialized = true;
		dev_info(dev, "RTL8372N core already initialized; reusing state\n");
		return 0;
	}

	priv->switch_touched = true;
	ret = rtl8372n_modify_reg(mdiodev, 0x6330, 0x30000, 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, 0x6330, 0x00c0, 0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, 0x6334, 0x00f0, 0x00f0);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_modify_reg(mdiodev, 0x6454, 0x7000, 0x7000);
	if (ret)
		goto out_unlock;
	fsleep(1000);

	/* Vendor rtl8372n_init() primes both switch SerDes before reset flow. */
	ret = rtl8372n_sds_modify(mdiodev, 0, 0, 0, 0x0200, 0x0200);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_modify(mdiodev, 1, 0, 0, 0x0200, 0x0200);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_modify(mdiodev, 0, 6, 2, 0x2000, 0x2000);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_sds_modify(mdiodev, 1, 6, 2, 0x2000, 0x2000);
	if (ret)
		goto out_unlock;
	fsleep(5000);
	ret = rtl8372n_fw_reset_flow(mdiodev, 1);
	if (ret)
		goto out_unlock;
	fsleep(5000);
	ret = rtl8372n_fw_reset_flow(mdiodev, 0);
	if (ret)
		goto out_unlock;

	ret = rtl8372n_phy_write(mdiodev, 0xf0, 0x1f, 0xa610, 0x2858);
	if (ret)
		goto out_unlock;
	phy_disabled = true;
	ret = rtl8372n_modify_reg(mdiodev, 0x5fd4, 0x180000, 0x180000);
	if (ret)
		goto out_unlock;
	for (port = 3; port <= 8; port++) {
		reg = 0x1238 + port * 0x100;
		ret = rtl8372n_modify_reg(mdiodev, reg, BIT(4) | BIT(8),
					  BIT(4) | BIT(8));
		if (ret)
			goto out_unlock;
	}
	ret = rtl8372n_modify_reg(mdiodev, 0x0b7c, BIT(5), BIT(5));
	if (ret)
		goto out_unlock;
	for (reg = 0x7124; reg < 0x714c; reg += 4) {
		ret = rtl8372n_write_reg(mdiodev, reg, 0x1050);
		if (ret)
			goto out_unlock;
	}
	ret = rtl8372n_modify_reg(mdiodev, 0x6040, BIT(0), BIT(0));
	if (ret)
		goto out_unlock;
	msleep(100);

	/* Firmware-version-conditioned RTCT/AFE patches remain deferred. */
	ret = rtl8372n_phy_write(mdiodev, 0xf0, 0x1f, 0xa610, 0x2058);
	if (ret)
		goto out_unlock;
	phy_disabled = false;
	ret = rtl8372n_modify_reg(mdiodev, 0x632c, 0x1ff000, 0x1f8000);
	if (ret)
		goto out_unlock;
	msleep(50);

out_unlock:
	if (ret && phy_disabled) {
		recovery_ret = rtl8372n_phy_write(mdiodev, 0xf0, 0x1f,
						  0xa610, 0x2058);
		if (recovery_ret)
			dev_err(dev, "failed to re-enable RTL8372N PHYs: %d\n",
				recovery_ret);
	}
	mutex_unlock(&mdiodev->bus->mdio_lock);
	if (ret)
		return dev_err_probe(dev, ret,
				     "RTL8372N minimal core init failed; probe unwind will reset the switch\n");

	priv->switch_initialized = true;
	dev_info(dev, "RTL8372N core initialized\n");
	return 0;
}

static int zx279133_lan_xpcs_read(struct zx279133_rtl8372n *priv,
				  int devad, u16 reg)
{
	return mdiodev_c45_read(priv->xpcs_mdiodev, devad, reg);
}

static int zx279133_lan_xpcs_write(struct zx279133_rtl8372n *priv,
				   int devad, u16 reg, u16 val)
{
	return mdiodev_c45_write(priv->xpcs_mdiodev, devad, reg, val);
}

static int zx279133_lan_xpcs_modify(struct zx279133_rtl8372n *priv,
				    int devad, u16 reg, u16 mask, u16 set)
{
	int val;

	val = zx279133_lan_xpcs_read(priv, devad, reg);
	if (val < 0)
		return val;

	return zx279133_lan_xpcs_write(priv, devad, reg,
					 (val & ~mask) | (set & mask));
}

static void zx279133_lan_xpcs_restore(struct zx279133_rtl8372n *priv)
{
	if (!priv->xpcs_configured)
		return;

	zx279133_lan_xpcs_write(priv, MDIO_MMD_VEND2,
				ZX279133_XPCS_VR_MII_AN_CTRL,
				 priv->saved_vend2_an_ctrl);
	zx279133_lan_xpcs_write(priv, MDIO_MMD_VEND2, MDIO_CTRL1,
				priv->saved_vend2_bmcr);
	zx279133_lan_xpcs_write(priv, MDIO_MMD_PCS,
				ZX279133_XPCS_VR_XS_PCS_KR_CTRL,
				 priv->saved_pcs_kr_ctrl);
	zx279133_lan_xpcs_write(priv, MDIO_MMD_PCS,
				ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1,
				 priv->saved_pcs_dig1);
	zx279133_lan_xpcs_write(priv, MDIO_MMD_PCS, MDIO_CTRL2,
				priv->saved_pcs_ctrl2);
	priv->xpcs_configured = false;
}

static int zx279133_lan_xpcs_configure(struct zx279133_rtl8372n *priv)
{
	int val;
	int ret;

	val = zx279133_lan_xpcs_read(priv, MDIO_MMD_PCS, MDIO_CTRL2);
	if (val < 0)
		return val;
	priv->saved_pcs_ctrl2 = val;
	val = zx279133_lan_xpcs_read(priv, MDIO_MMD_PCS,
				     ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1);
	if (val < 0)
		return val;
	priv->saved_pcs_dig1 = val;
	val = zx279133_lan_xpcs_read(priv, MDIO_MMD_PCS,
				     ZX279133_XPCS_VR_XS_PCS_KR_CTRL);
	if (val < 0)
		return val;
	priv->saved_pcs_kr_ctrl = val;
	val = zx279133_lan_xpcs_read(priv, MDIO_MMD_VEND2, MDIO_CTRL1);
	if (val < 0)
		return val;
	priv->saved_vend2_bmcr = val;
	val = zx279133_lan_xpcs_read(priv, MDIO_MMD_VEND2,
				     ZX279133_XPCS_VR_MII_AN_CTRL);
	if (val < 0)
		return val;
	priv->saved_vend2_an_ctrl = val;
	priv->xpcs_configured = true;

	ret = read_poll_timeout(zx279133_lan_xpcs_read, val,
				val >= 0 && !(val & ZX279133_XPCS_VR_RST),
				1000, 400000, false, priv,
				MDIO_MMD_PCS, MDIO_CTRL1);
	if (ret)
		goto err_restore;
	ret = read_poll_timeout(zx279133_lan_xpcs_read, val,
				val >= 0 && !(val & ZX279133_XPCS_VR_RST),
				1000, 400000, false, priv,
				MDIO_MMD_VEND2, MDIO_CTRL1);
	if (ret)
		goto err_restore;

	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_VEND2,
				       ZX279133_XPCS_VR_MII_AN_CTRL,
					BIT(3) | BIT(8), 0);
	if (ret)
		goto err_restore;
	ret = zx279133_lan_xpcs_write(priv, MDIO_MMD_PCS, MDIO_CTRL2, 0);
	if (ret)
		goto err_restore;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_PCS,
				       ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1,
					ZX279133_XPCS_USXG_EN |
					ZX279133_XPCS_VSMMD1_EN,
					ZX279133_XPCS_USXG_EN |
					ZX279133_XPCS_VSMMD1_EN);
	if (ret)
		goto err_restore;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_PCS,
				       ZX279133_XPCS_VR_XS_PCS_KR_CTRL,
					ZX279133_XPCS_USXG_MODE_MASK, 0);
	if (ret)
		goto err_restore;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_PCS,
				       ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1,
					ZX279133_XPCS_VR_RST,
					ZX279133_XPCS_VR_RST);
	if (ret)
		goto err_restore;
	ret = read_poll_timeout(zx279133_lan_xpcs_read, val,
				val >= 0 && !(val & ZX279133_XPCS_VR_RST),
				1000, 400000, false, priv, MDIO_MMD_PCS,
				ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1);
	if (ret)
		goto err_restore;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_VEND2,
				       ZX279133_XPCS_VR_MII_AN_CTRL,
					ZX279133_XPCS_AN_INTR_EN,
					ZX279133_XPCS_AN_INTR_EN);
	if (ret)
		goto err_restore;
	ret = zx279133_lan_xpcs_write(priv, MDIO_MMD_VEND2, MDIO_CTRL1,
				      ZX279133_XPCS_USXGMII_AN_BMCR);
	if (ret)
		goto err_restore;

	return 0;

err_restore:
	zx279133_lan_xpcs_restore(priv);
	return ret;
}

static int
zx279133_lan_xpcs_reapply_after_switch(struct zx279133_rtl8372n *priv)
{
	u32 status;
	int val;
	int ret;

	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_VEND2,
				       ZX279133_XPCS_VR_MII_AN_CTRL,
					BIT(3) | BIT(8), 0);
	if (ret)
		return ret;
	ret = zx279133_lan_xpcs_write(priv, MDIO_MMD_PCS, MDIO_CTRL2, 0);
	if (ret)
		return ret;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_PCS,
				       ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1,
					ZX279133_XPCS_USXG_EN |
					ZX279133_XPCS_VSMMD1_EN,
					ZX279133_XPCS_USXG_EN |
					ZX279133_XPCS_VSMMD1_EN);
	if (ret)
		return ret;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_PCS,
				       ZX279133_XPCS_VR_XS_PCS_KR_CTRL,
					ZX279133_XPCS_USXG_MODE_MASK, 0);
	if (ret)
		return ret;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_PCS,
				       ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1,
					ZX279133_XPCS_VR_RST,
					ZX279133_XPCS_VR_RST);
	if (ret)
		return ret;
	ret = read_poll_timeout(zx279133_lan_xpcs_read, val,
				val >= 0 && !(val & ZX279133_XPCS_VR_RST),
				1000, 400000, false, priv, MDIO_MMD_PCS,
				ZX279133_XPCS_VR_XS_PCS_DIG_CTRL1);
	if (ret)
		return ret;

	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_VEND2,
				       ZX279133_XPCS_VR_MII_AN_CTRL,
					ZX279133_XPCS_AN_INTR_EN, 0);
	if (ret)
		return ret;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_VEND2, MDIO_CTRL1,
				       ZX279133_XPCS_AN_ENABLE, 0);
	if (ret)
		return ret;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_PCS,
				       ZX279133_XPCS_BYPASS_REG,
					ZX279133_XPCS_BYPASS_EN,
					ZX279133_XPCS_BYPASS_EN);
	if (ret)
		return ret;

	return read_poll_timeout(zx279133_lan_nppt_read, status,
				 (status & 0x3800) == 0x3800, 1000, 400000,
				 false, priv, ZX279133_NPPT_XPCS0_STATUS);
}

static void zx279133_lan_datapath_restore(struct zx279133_rtl8372n *priv)
{
	u32 xmac = ZX279133_XMAC0_BASE;
	u32 value;

	if (!priv->datapath_enabled)
		return;

	zx279133_lan_xmac_lock(priv);
	value = zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_RX_CTRL);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_RX_CTRL,
				value & ~BIT(0));
	value = zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_TX_CTRL);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_TX_CTRL,
				value & ~BIT(0));
	zx279133_lan_nppt_write(priv, ZX279133_XMAC0_SOPC_SEND,
				priv->saved_sopc_send);
	zx279133_lan_nppt_write(priv, ZX279133_XMAC_SOPC_DUPLEX,
				priv->saved_sopc_duplex);
	zx279133_lan_xmac_unlock(priv);
	zx279133_lan_xpcs_write(priv, MDIO_MMD_PCS,
				ZX279133_XPCS_BYPASS_REG,
				 priv->saved_xpcs_bypass);
	priv->datapath_enabled = false;
}

static int zx279133_lan_datapath_enable(struct zx279133_rtl8372n *priv)
{
	u32 xmac = ZX279133_XMAC0_BASE;
	int val;
	int ret;
	u32 value;

	/* Vendor SR1010 tail disables XPCS0 AN after mode-5 auto setup. */
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_VEND2,
				       ZX279133_XPCS_VR_MII_AN_CTRL,
					ZX279133_XPCS_AN_INTR_EN, 0);
	if (ret)
		return ret;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_VEND2, MDIO_CTRL1,
				       ZX279133_XPCS_AN_ENABLE, 0);
	if (ret)
		return ret;

	val = zx279133_lan_xpcs_read(priv, MDIO_MMD_PCS,
				     ZX279133_XPCS_BYPASS_REG);
	if (val < 0)
		return val;
	priv->saved_xpcs_bypass = val;
	ret = zx279133_lan_xpcs_modify(priv, MDIO_MMD_PCS,
				       ZX279133_XPCS_BYPASS_REG,
					ZX279133_XPCS_BYPASS_EN,
					ZX279133_XPCS_BYPASS_EN);
	if (ret)
		return ret;

	zx279133_lan_xmac_lock(priv);
	priv->saved_sopc_duplex =
		zx279133_lan_nppt_read(priv, ZX279133_XMAC_SOPC_DUPLEX);
	priv->saved_sopc_send =
		zx279133_lan_nppt_read(priv, ZX279133_XMAC0_SOPC_SEND);
	value = priv->saved_sopc_duplex & ~ZX279133_XMAC0_SOPC_DUPLEX_MASK;
	zx279133_lan_nppt_write(priv, ZX279133_XMAC_SOPC_DUPLEX, value);
	usleep_range(4295, 4395);
	zx279133_lan_nppt_write(priv, ZX279133_XMAC0_SOPC_SEND, 1);
	value = zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_RX_CTRL);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_RX_CTRL,
				value | BIT(0));
	value = zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_TX_CTRL);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_TX_CTRL,
				value | BIT(0));
	zx279133_lan_xmac_unlock(priv);
	priv->datapath_enabled = true;

	return 0;
}

static void zx279133_lan_xmac_restore(struct zx279133_rtl8372n *priv)
{
	u32 xmac = ZX279133_XMAC0_BASE;

	if (!priv->xmac_configured)
		return;

	zx279133_lan_xmac_lock(priv);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_TX_CTRL,
				priv->saved_xmac_tx);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_RX_CTRL,
				priv->saved_xmac_rx);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_FRAME_CFG,
				priv->saved_xmac_frame);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_MODE_CFG,
				priv->saved_xmac_mode);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_DUPLEX,
				priv->saved_xmac_duplex);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_MISC_CFG,
				priv->saved_xmac_misc);
	zx279133_lan_nppt_write(priv, ZX279133_XMAC_RESET_REG,
				priv->saved_xmac_reset);
	zx279133_lan_xmac_unlock(priv);
	priv->xmac_configured = false;
}

static void zx279133_lan_xmac_configure(struct zx279133_rtl8372n *priv)
{
	u32 xmac = ZX279133_XMAC0_BASE;
	u32 value;

	zx279133_lan_xmac_lock(priv);
	priv->saved_xmac_tx =
		zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_TX_CTRL);
	priv->saved_xmac_rx =
		zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_RX_CTRL);
	priv->saved_xmac_frame =
		zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_FRAME_CFG);
	priv->saved_xmac_mode =
		zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_MODE_CFG);
	priv->saved_xmac_duplex =
		zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_DUPLEX);
	priv->saved_xmac_misc =
		zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_MISC_CFG);
	priv->saved_xmac_reset =
		zx279133_lan_nppt_read(priv, ZX279133_XMAC_RESET_REG);
	priv->xmac_configured = true;

	zx279133_lan_nppt_write(priv, ZX279133_XMAC_RESET_REG,
				priv->saved_xmac_reset & ~ZX279133_XMAC0_RESET);
	usleep_range(ZX279133_XMAC_RESET_US, ZX279133_XMAC_RESET_US + 100);
	zx279133_lan_nppt_write(priv, ZX279133_XMAC_RESET_REG,
				priv->saved_xmac_reset | ZX279133_XMAC0_RESET);

	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_TX_CTRL,
				0x00010000);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_RX_CTRL,
				0x3e800086);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_FRAME_CFG,
				0x80000001);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_MODE_CFG,
				0x00000002);
	value = zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_DUPLEX);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_DUPLEX,
				value & ~ZX279133_XMAC_HALF_DUPLEX);
	value = zx279133_lan_nppt_read(priv, xmac + ZX279133_XMAC_MISC_CFG);
	zx279133_lan_nppt_write(priv, xmac + ZX279133_XMAC_MISC_CFG,
				value | BIT(9));
	zx279133_lan_xmac_unlock(priv);
}

static enum dsa_tag_protocol
zx279133_rtl8372n_get_tag_protocol(struct dsa_switch *ds, int port,
				   enum dsa_tag_protocol conduit_proto)
{
	return DSA_TAG_PROTO_ZX279133_RTL8372N;
}

static int zx279133_rtl8372n_setup(struct dsa_switch *ds)
{
	/* Probe completes the reset, CPU-link and transport setup first. */
	return 0;
}

static int
zx279133_rtl8372n_port_bridge_join(struct dsa_switch *ds, int port,
				   struct dsa_bridge bridge,
				  bool *tx_fwd_offload,
				  struct netlink_ext_ack *extack)
{
	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return -EOPNOTSUPP;

	/* Private transport VLANs remain isolated to one user port plus CPU8.
	 * Linux owns all bridge forwarding and customer VLAN filtering.
	 */
	*tx_fwd_offload = false;
	dev_info(ds->dev, "port %d joined bridge %s\n", port,
		 bridge.dev->name);
	return 0;
}

static void
zx279133_rtl8372n_port_bridge_leave(struct dsa_switch *ds, int port,
				    struct dsa_bridge bridge)
{
	struct zx279133_rtl8372n *priv = ds->priv;

	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return;

	clear_bit(port, &priv->vlan_unaware_vid1_mask);
	dev_info(ds->dev, "port %d left bridge %s\n", port,
		 bridge.dev->name);
}

static bool
rtl8372n_port_in_vlan_unaware_bridge(struct dsa_switch *ds, int port)
{
	struct net_device *bridge;

	bridge = dsa_port_bridge_dev_get(dsa_to_port(ds, port));
	return bridge && !br_vlan_enabled(bridge);
}

static int
zx279133_rtl8372n_port_vlan_filtering(struct dsa_switch *ds, int port,
				      bool vlan_filtering,
				     struct netlink_ext_ack *extack)
{
	struct zx279133_rtl8372n *priv = ds->priv;

	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return -EOPNOTSUPP;
	if (vlan_filtering &&
	    test_bit(port, &priv->vlan_unaware_vid1_mask)) {
		NL_SET_ERR_MSG_MOD(extack,
				   "VID 1 is reserved; set vlan_default_pvid 0 before enabling VLAN filtering");
		return -EBUSY;
	}

	/* Customer VLANs are nested inside the fixed per-port CPU-link VLAN.
	 * Keep the validated transport PVID and hardware filtering policy intact;
	 * the Linux bridge applies the customer VLAN policy after the DSA tagger
	 * removes the private outer tag.
	 */
	if (vlan_filtering)
		set_bit(port, &priv->vlan_filtering_mask);
	else
		clear_bit(port, &priv->vlan_filtering_mask);
	dev_dbg(ds->dev, "software VLAN filtering port %d=%u mask=%#lx\n",
		port, vlan_filtering, priv->vlan_filtering_mask);
	return 0;
}

static int rtl8372n_customer_vlan_check(u16 vid,
					struct netlink_ext_ack *extack)
{
	if (!vid || vid >= VLAN_N_VID) {
		NL_SET_ERR_MSG_MOD(extack, "VLAN ID out of range");
		return -EINVAL;
	}
	if (vid == 1) {
		NL_SET_ERR_MSG_MOD(extack,
				   "VID 1 is reserved; create the bridge with vlan_default_pvid 0");
		return -EBUSY;
	}
	if (rtl8372n_is_transport_vid(vid)) {
		NL_SET_ERR_MSG_MOD(extack,
				   "VLAN ID reserved by the ZX279133 private DSA transport");
		return -EBUSY;
	}
	return 0;
}

static int
zx279133_rtl8372n_port_vlan_add(struct dsa_switch *ds, int port,
				const struct switchdev_obj_port_vlan *vlan,
			       struct netlink_ext_ack *extack)
{
	struct zx279133_rtl8372n *priv = ds->priv;
	struct mdio_device *mdiodev = priv->switch_mdiodev;
	bool old_pvid_valid;
	u16 old_bridge_pvid;
	u16 old_hw_pvid;
	u32 old_entry;
	u32 new_entry;
	u32 readback;
	u32 members;
	u32 untag;
	int ret;

	if (port == RTL8372N_CPU_PORT)
		return 0;
	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return -EINVAL;

	/* A VLAN-unaware bridge still publishes its bookkeeping PVID 1 through
	 * switchdev. It has no data-path meaning until VLAN filtering is enabled,
	 * so leave the private transport PVID and VLAN table untouched.
	 */
	if (vlan->vid == 1 &&
	    rtl8372n_port_in_vlan_unaware_bridge(ds, port)) {
		set_bit(port, &priv->vlan_unaware_vid1_mask);
		dev_dbg(ds->dev, "ignored VLAN-unaware VID 1 add on port %d\n",
			port);
		return 0;
	}

	ret = rtl8372n_customer_vlan_check(vlan->vid, extack);
	if (ret)
		return ret;
	if (!(vlan->flags & BRIDGE_VLAN_INFO_PVID) ||
	    !(vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED)) {
		NL_SET_ERR_MSG_MOD(extack,
				   "only untagged access VLANs are supported over the private CPU-link transport");
		return -EOPNOTSUPP;
	}

	mutex_lock(&mdiodev->bus->mdio_lock);
	ret = rtl8372n_vlan_read(mdiodev, vlan->vid, &old_entry);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_port_pvid_read(mdiodev, port, &old_hw_pvid);
	if (ret)
		goto out_unlock;
	old_pvid_valid = priv->bridge_pvid_valid[port];
	old_bridge_pvid = priv->bridge_pvid[port];
	if (old_pvid_valid && old_bridge_pvid != vlan->vid) {
		NL_SET_ERR_MSG_MOD(extack,
				   "delete the current access VLAN before selecting another PVID");
		ret = -EBUSY;
		goto out_unlock;
	}

	members = (old_entry & RTL8372N_VLAN_MBR_MASK) |
		  BIT(port) | BIT(RTL8372N_CPU_PORT);
	untag = (old_entry & RTL8372N_VLAN_UNTAG_MASK) >>
		RTL8372N_VLAN_UNTAG_SHIFT;
	if (vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED)
		untag |= BIT(port);
	else
		untag &= ~BIT(port);
	untag &= ~BIT(RTL8372N_CPU_PORT);
	new_entry = old_entry & ~(RTL8372N_VLAN_MBR_MASK |
				 RTL8372N_VLAN_UNTAG_MASK);
	new_entry |= members | (untag << RTL8372N_VLAN_UNTAG_SHIFT) |
		     RTL8372N_VLAN_IVL;

	ret = rtl8372n_vlan_write(mdiodev, vlan->vid, new_entry);
	if (ret)
		goto restore;
	if (vlan->flags & BRIDGE_VLAN_INFO_PVID) {
		priv->bridge_pvid[port] = vlan->vid;
		priv->bridge_pvid_valid[port] = true;
	}
	ret = rtl8372n_vlan_read(mdiodev, vlan->vid, &readback);
	if (ret)
		goto restore;
	if (readback != new_entry) {
		ret = -EIO;
		goto restore;
	}
	if (vlan->flags & BRIDGE_VLAN_INFO_PVID) {
		ret = zx279133_tagger_set_access_vlan(ds, port, vlan->vid,
						      !!(vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED));
		if (ret)
			goto restore;
	}
	dev_dbg(ds->dev, "access VLAN add port %d vid %u entry=%#x\n",
		port, vlan->vid, readback);
	goto out_unlock;

restore:
	priv->bridge_pvid_valid[port] = old_pvid_valid;
	priv->bridge_pvid[port] = old_bridge_pvid;
	rtl8372n_vlan_write(mdiodev, vlan->vid, old_entry);
	rtl8372n_port_pvid_write(mdiodev, port, old_hw_pvid);
out_unlock:
	mutex_unlock(&mdiodev->bus->mdio_lock);
	return ret;
}

static int
zx279133_rtl8372n_port_vlan_del(struct dsa_switch *ds, int port,
				const struct switchdev_obj_port_vlan *vlan)
{
	struct zx279133_rtl8372n *priv = ds->priv;
	struct mdio_device *mdiodev = priv->switch_mdiodev;
	bool old_pvid_valid;
	u16 old_bridge_pvid;
	u16 old_hw_pvid;
	u32 old_entry;
	u32 new_entry;
	u32 readback;
	u32 members;
	u32 untag;
	int ret;

	if (port == RTL8372N_CPU_PORT)
		return 0;
	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return -EINVAL;

	if (vlan->vid == 1 &&
	    rtl8372n_port_in_vlan_unaware_bridge(ds, port)) {
		clear_bit(port, &priv->vlan_unaware_vid1_mask);
		dev_dbg(ds->dev, "ignored VLAN-unaware VID 1 delete on port %d\n",
			port);
		return 0;
	}

	if (!vlan->vid || vlan->vid >= VLAN_N_VID ||
	    vlan->vid == 1 || rtl8372n_is_transport_vid(vlan->vid))
		return -EINVAL;

	mutex_lock(&mdiodev->bus->mdio_lock);
	ret = rtl8372n_vlan_read(mdiodev, vlan->vid, &old_entry);
	if (ret)
		goto out_unlock;
	ret = rtl8372n_port_pvid_read(mdiodev, port, &old_hw_pvid);
	if (ret)
		goto out_unlock;
	old_pvid_valid = priv->bridge_pvid_valid[port];
	old_bridge_pvid = priv->bridge_pvid[port];

	members = (old_entry & RTL8372N_VLAN_MBR_MASK) & ~BIT(port);
	untag = ((old_entry & RTL8372N_VLAN_UNTAG_MASK) >>
		 RTL8372N_VLAN_UNTAG_SHIFT) & ~BIT(port);
	if (!(members & RTL8372N_USER_PORT_MASK))
		members &= ~BIT(RTL8372N_CPU_PORT);
	if (!members) {
		new_entry = 0;
	} else {
		new_entry = old_entry & ~(RTL8372N_VLAN_MBR_MASK |
					 RTL8372N_VLAN_UNTAG_MASK);
		new_entry |= members | (untag << RTL8372N_VLAN_UNTAG_SHIFT);
	}

	ret = rtl8372n_vlan_write(mdiodev, vlan->vid, new_entry);
	if (ret)
		goto restore;
	if (priv->bridge_pvid_valid[port] &&
	    priv->bridge_pvid[port] == vlan->vid)
		priv->bridge_pvid_valid[port] = false;
	ret = rtl8372n_vlan_read(mdiodev, vlan->vid, &readback);
	if (ret)
		goto restore;
	if (readback != new_entry) {
		ret = -EIO;
		goto restore;
	}
	if (old_pvid_valid && old_bridge_pvid == vlan->vid) {
		ret = zx279133_tagger_set_access_vlan(ds, port, 0, false);
		if (ret)
			goto restore;
	}
	goto out_unlock;

restore:
	priv->bridge_pvid_valid[port] = old_pvid_valid;
	priv->bridge_pvid[port] = old_bridge_pvid;
	rtl8372n_vlan_write(mdiodev, vlan->vid, old_entry);
	rtl8372n_port_pvid_write(mdiodev, port, old_hw_pvid);
out_unlock:
	mutex_unlock(&mdiodev->bus->mdio_lock);
	return ret;
}

static void zx279133_rtl8372n_port_stp_state_set(struct dsa_switch *ds,
						 int port, u8 state)
{
	struct zx279133_rtl8372n *priv = ds->priv;
	u32 hw_state;
	u32 value;
	int ret;

	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return;

	switch (state) {
	case BR_STATE_DISABLED:
		hw_state = 0;
		break;
	case BR_STATE_BLOCKING:
	case BR_STATE_LISTENING:
		hw_state = 1;
		break;
	case BR_STATE_LEARNING:
		hw_state = 2;
		break;
	case BR_STATE_FORWARDING:
		hw_state = 3;
		break;
	default:
		dev_err(ds->dev, "unsupported STP state %u for port %d\n",
			state, port);
		return;
	}

	mutex_lock(&priv->switch_mdiodev->bus->mdio_lock);
	ret = rtl8372n_modify_reg(priv->switch_mdiodev,
				  RTL8372N_MSTP0_STATE,
				  GENMASK(port * 2 + 1, port * 2),
				  hw_state << (port * 2));
	if (!ret)
		ret = rtl8372n_read_reg(priv->switch_mdiodev,
					RTL8372N_MSTP0_STATE, &value);
	mutex_unlock(&priv->switch_mdiodev->bus->mdio_lock);
	if (ret) {
		dev_err(ds->dev, "failed to set STP state %u on port %d: %d\n",
			state, port, ret);
		return;
	}
	if (((value >> (port * 2)) & 0x3) != hw_state)
		dev_err(ds->dev,
			"STP state readback mismatch on port %d: reg=%#x expected=%u\n",
			port, value, hw_state);
}

static int zx279133_rtl8372n_port_fdb_add(struct dsa_switch *ds, int port,
					  const unsigned char *addr,
					  u16 vid, struct dsa_db db)
{
	struct zx279133_rtl8372n *priv = ds->priv;
	u32 words[3];
	u32 readback[3];
	u16 fid = vid ? vid : 1;
	bool ivl = !!vid;
	int ret;

	if (port == RTL8372N_CPU_PORT)
		return 0;
	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return -EINVAL;
	if (!is_unicast_ether_addr(addr) || vid > 4095)
		return -EINVAL;
	if (vid && test_bit(port, &priv->vlan_filtering_mask))
		return -EOPNOTSUPP;

	rtl8372n_l2_encode_key(addr, fid, ivl, words);
	words[1] |= (u32)(port & 0x3) << 30;
	words[2] = (port >> 2) | (6 << 2) | BIT(5) | BIT(16);

	mutex_lock(&priv->switch_mdiodev->bus->mdio_lock);
	ret = rtl8372n_l2_write_words(priv->switch_mdiodev, words);
	if (!ret)
		ret = rtl8372n_l2_lookup(priv->switch_mdiodev, addr, fid, ivl,
					 readback);
	mutex_unlock(&priv->switch_mdiodev->bus->mdio_lock);
	if (ret)
		return ret;
	if (readback[0] != words[0] || readback[1] != words[1] ||
	    readback[2] != words[2]) {
		dev_err(ds->dev,
			"FDB add readback mismatch for %pM vid %u port %d: %08x/%08x/%08x\n",
			addr, vid, port, readback[0], readback[1], readback[2]);
		return -EIO;
	}

	return 0;
}

static int zx279133_rtl8372n_port_fdb_del(struct dsa_switch *ds, int port,
					  const unsigned char *addr,
					  u16 vid, struct dsa_db db)
{
	struct zx279133_rtl8372n *priv = ds->priv;
	u32 words[3];
	u16 fid = vid ? vid : 1;
	bool ivl = !!vid;
	int ret;

	if (port == RTL8372N_CPU_PORT)
		return 0;
	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return -EINVAL;
	if (!is_unicast_ether_addr(addr) || vid > 4095)
		return -EINVAL;
	if (vid && test_bit(port, &priv->vlan_filtering_mask))
		return -EOPNOTSUPP;

	mutex_lock(&priv->switch_mdiodev->bus->mdio_lock);
	ret = rtl8372n_l2_lookup(priv->switch_mdiodev, addr, fid, ivl, words);
	if (ret == -ENOENT) {
		ret = 0;
		goto out_unlock;
	}
	if (ret)
		goto out_unlock;

	rtl8372n_l2_encode_key(addr, fid, ivl, words);
	ret = rtl8372n_l2_write_words(priv->switch_mdiodev, words);
	if (!ret) {
		ret = rtl8372n_l2_lookup(priv->switch_mdiodev, addr, fid, ivl,
					 words);
		if (ret == -ENOENT)
			ret = 0;
		else if (!ret)
			ret = -EIO;
	}

out_unlock:
	mutex_unlock(&priv->switch_mdiodev->bus->mdio_lock);
	return ret;
}

static int zx279133_rtl8372n_port_fdb_dump(struct dsa_switch *ds, int port,
					   dsa_fdb_dump_cb_t *cb,
					   void *data)
{
	struct zx279133_rtl8372n *priv = ds->priv;
	u32 words[3];
	unsigned char addr[ETH_ALEN];
	u16 start = 0;
	u16 entry;
	u16 fid;
	u16 vid;
	unsigned int spa;
	bool is_static;
	bool ivl;
	int ret = 0;

	if (port < RTL8372N_USER_PORT_MIN || port > RTL8372N_USER_PORT_MAX)
		return -EINVAL;
	if (test_bit(port, &priv->vlan_filtering_mask))
		return 0;

	mutex_lock(&priv->switch_mdiodev->bus->mdio_lock);
	for (;;) {
		ret = rtl8372n_l2_next_uc(priv->switch_mdiodev, start, &entry,
					  words);
		if (ret == -ENOENT) {
			ret = 0;
			break;
		}
		if (ret)
			break;

		spa = ((words[1] >> 30) & 0x3) | ((words[2] & 0x3) << 2);
		fid = (words[1] >> 16) & 0xfff;
		ivl = !!(words[1] & BIT(29));
		if (spa == port && (ivl || fid == 1)) {
			addr[5] = words[0];
			addr[4] = words[0] >> 8;
			addr[3] = words[0] >> 16;
			addr[2] = words[0] >> 24;
			addr[1] = words[1];
			addr[0] = words[1] >> 8;
			vid = ivl ? fid : 0;
			is_static = !!(words[2] & BIT(16));
			ret = cb(addr, vid, is_static, data);
			if (ret)
				break;
		}

		if (entry == RTL8372N_L2_MAX_ADDRESS)
			break;
		start = entry + 1;
	}
	mutex_unlock(&priv->switch_mdiodev->bus->mdio_lock);

	return ret;
}

static void
zx279133_rtl8372n_phylink_fixed_state(struct dsa_switch *ds, int port,
				      struct phylink_link_state *state)
{
	struct zx279133_rtl8372n *priv = ds->priv;
	u32 link;
	u32 speed;
	u32 duplex;
	int ret;

	if (port == 8) {
		state->link = true;
		state->speed = SPEED_10000;
		state->duplex = DUPLEX_FULL;
		return;
	}
	if (port < RTL8372N_USER_PORT_MIN ||
	    port > RTL8372N_USER_PORT_MAX || !priv->switch_mdiodev) {
		state->link = false;
		return;
	}

	mutex_lock(&priv->switch_mdiodev->bus->mdio_lock);
	ret = rtl8372n_read_reg(priv->switch_mdiodev,
				RTL8372N_LINK_STATUS, &link);
	if (!ret)
		ret = rtl8372n_read_reg(priv->switch_mdiodev,
					RTL8372N_SPEED_STATUS0, &speed);
	if (!ret)
		ret = rtl8372n_read_reg(priv->switch_mdiodev,
					RTL8372N_DUPLEX_STATUS, &duplex);
	mutex_unlock(&priv->switch_mdiodev->bus->mdio_lock);
	if (ret) {
		state->link = false;
		return;
	}

	state->link = !!(link & BIT(port));
	state->duplex = duplex & BIT(port) ? DUPLEX_FULL : DUPLEX_HALF;
	switch ((speed >> ((port & 7) * 4)) & 0xf) {
	case 0:
		state->speed = SPEED_10;
		break;
	case 1:
		state->speed = SPEED_100;
		break;
	case 2:
		state->speed = SPEED_1000;
		break;
	case 4:
		state->speed = SPEED_10000;
		break;
	case 5:
		state->speed = SPEED_2500;
		break;
	case 6:
		state->speed = SPEED_5000;
		break;
	default:
		state->link = false;
		state->speed = SPEED_UNKNOWN;
		break;
	}
}

static void zx279133_rtl8372n_phylink_get_caps(struct dsa_switch *ds,
					       int port,
					       struct phylink_config *config)
{
	if (port >= RTL8372N_USER_PORT_MIN &&
	    port <= RTL8372N_USER_PORT_MAX) {
		__set_bit(PHY_INTERFACE_MODE_INTERNAL,
			  config->supported_interfaces);
		config->mac_capabilities = MAC_2500FD | MAC_SYM_PAUSE |
					   MAC_ASYM_PAUSE;
	} else if (port == 8) {
		__set_bit(PHY_INTERFACE_MODE_10GBASER,
			  config->supported_interfaces);
		config->mac_capabilities = MAC_10000FD | MAC_SYM_PAUSE |
					   MAC_ASYM_PAUSE;
	}
}

struct zx279133_rtl8372n_stat {
	char name[ETH_GSTRING_LEN];
	unsigned int counter;
};

static const struct zx279133_rtl8372n_stat zx279133_rtl8372n_stats[] = {
	{ "hw_tx_good_frames", RTL8372N_MIB_TX_GOOD_HIGH },
	{ "hw_rx_good_frames", RTL8372N_MIB_RX_GOOD_HIGH },
	{ "phy_tx_good_frames", RTL8372N_MIB_TX_GOOD_PHY_HIGH },
	{ "phy_rx_good_frames", RTL8372N_MIB_RX_GOOD_PHY_HIGH },
};

static int zx279133_rtl8372n_get_sset_count(struct dsa_switch *ds, int port,
					    int sset)
{
	if (sset != ETH_SS_STATS)
		return 0;

	return ARRAY_SIZE(zx279133_rtl8372n_stats);
}

static void zx279133_rtl8372n_get_strings(struct dsa_switch *ds, int port,
					  u32 stringset, u8 *data)
{
	unsigned int i;

	if (stringset != ETH_SS_STATS)
		return;

	for (i = 0; i < ARRAY_SIZE(zx279133_rtl8372n_stats); i++)
		ethtool_puts(&data, zx279133_rtl8372n_stats[i].name);
}

static void zx279133_rtl8372n_get_ethtool_stats(struct dsa_switch *ds,
						int port, u64 *data)
{
	struct zx279133_rtl8372n *priv = ds->priv;
	unsigned int i;
	int first_error = 0;
	int ret;

	memset(data, 0, sizeof(*data) * ARRAY_SIZE(zx279133_rtl8372n_stats));
	if (!priv->switch_mdiodev || port > 8)
		return;

	mutex_lock(&priv->switch_mdiodev->bus->mdio_lock);
	for (i = 0; i < ARRAY_SIZE(zx279133_rtl8372n_stats); i++) {
		ret = rtl8372n_mib_read(priv->switch_mdiodev, port,
					zx279133_rtl8372n_stats[i].counter,
					 &data[i]);
		if (ret && !first_error)
			first_error = ret;
	}
	mutex_unlock(&priv->switch_mdiodev->bus->mdio_lock);

	if (first_error)
		dev_warn_ratelimited(ds->dev,
				     "failed to read port %d MIB counters: %d\n",
				     port, first_error);
}

static const struct dsa_switch_ops zx279133_rtl8372n_dsa_ops = {
	.get_tag_protocol = zx279133_rtl8372n_get_tag_protocol,
	.setup = zx279133_rtl8372n_setup,
	.port_bridge_join = zx279133_rtl8372n_port_bridge_join,
	.port_bridge_leave = zx279133_rtl8372n_port_bridge_leave,
	.port_vlan_filtering = zx279133_rtl8372n_port_vlan_filtering,
	.port_vlan_add = zx279133_rtl8372n_port_vlan_add,
	.port_vlan_del = zx279133_rtl8372n_port_vlan_del,
	.port_stp_state_set = zx279133_rtl8372n_port_stp_state_set,
	.port_fdb_add = zx279133_rtl8372n_port_fdb_add,
	.port_fdb_del = zx279133_rtl8372n_port_fdb_del,
	.port_fdb_dump = zx279133_rtl8372n_port_fdb_dump,
	.phylink_get_caps = zx279133_rtl8372n_phylink_get_caps,
	.phylink_fixed_state = zx279133_rtl8372n_phylink_fixed_state,
	.get_sset_count = zx279133_rtl8372n_get_sset_count,
	.get_strings = zx279133_rtl8372n_get_strings,
	.get_ethtool_stats = zx279133_rtl8372n_get_ethtool_stats,
};

static int zx279133_rtl8372n_dsa_register(struct device *dev,
					  struct zx279133_rtl8372n *priv)
{
	struct dsa_switch *ds;
	int ret;

	ds = devm_kzalloc(dev, sizeof(*ds), GFP_KERNEL);
	if (!ds)
		return -ENOMEM;

	ds->dev = dev;
	ds->num_ports = 9;
	ds->ops = &zx279133_rtl8372n_dsa_ops;
	ds->priv = priv;

	ret = dsa_register_switch(ds);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to register RTL8372N DSA switch\n");

	priv->ds = ds;
	priv->dsa_registered = true;
	dev_info(dev, "registered RTL8372N DSA switch\n");

	return 0;
}

static int zx279133_rtl8372n_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *parent_np = dev->parent->of_node;
	struct zx279133_rtl8372n *priv;
	struct device_node *pcs_np;
	struct device_node *switch_np;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->service = dev_get_drvdata(dev->parent);
	if (!priv->service)
		return dev_err_probe(dev, -EPROBE_DEFER,
				     "NPPT LAN service is not ready\n");
	if (!zx279133_lan_service_valid(priv->service))
		return dev_err_probe(dev, -EINVAL,
				     "incomplete NPPT LAN service\n");
	if (!parent_np)
		return dev_err_probe(dev, -ENODEV,
				     "NPPT parent device tree node is missing\n");

	ret = zx279133_lan_service_datapath_get(priv->service);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to acquire shared NPPT/IDM datapath\n");
	priv->parent_datapath_held = true;

	switch_np = of_parse_phandle(dev->of_node, "zte,mdio-handle", 0);
	if (!switch_np) {
		ret = -ENODEV;
		dev_err(dev, "RTL8372N MDIO phandle is missing\n");
		goto err_parent_datapath_put;
	}
	priv->switch_mdiodev =
		fwnode_mdio_find_device(of_fwnode_handle(switch_np));
	of_node_put(switch_np);
	if (!priv->switch_mdiodev) {
		ret = -EPROBE_DEFER;
		dev_err(dev, "RTL8372N MDIO device is unavailable\n");
		goto err_parent_datapath_put;
	}

	priv->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset_gpio)) {
		ret = dev_err_probe(dev, PTR_ERR(priv->reset_gpio),
				    "failed to acquire RTL8372N reset GPIO\n");
		priv->reset_gpio = NULL;
		goto err_switch_put;
	}
	priv->switch_reset_asserted = true;
	rtl8372n_hw_reset(dev, priv);

	priv->serdes = devm_of_phy_get(dev, parent_np, "lan-serdes");
	if (IS_ERR(priv->serdes)) {
		ret = dev_err_probe(dev, PTR_ERR(priv->serdes),
				    "Uni SerDes PHY is unavailable\n");
		goto err_switch_put;
	}

	ret = phy_set_mode_ext(priv->serdes, PHY_MODE_ETHERNET,
			       PHY_INTERFACE_MODE_USXGMII);
	if (ret) {
		dev_err_probe(dev, ret,
			      "failed to select Uni SerDes USXGMII mode\n");
		goto err_switch_put;
	}

	ret = phy_power_on(priv->serdes);
	if (ret) {
		dev_warn(dev, "Uni SerDes first lock attempt failed: %d; retrying\n",
			 ret);
		msleep(20);
		ret = phy_power_on(priv->serdes);
	}
	if (ret) {
		dev_err_probe(dev, ret,
			      "failed to power on Uni SerDes\n");
		goto err_switch_put;
	}
	priv->serdes_powered = true;

	pcs_np = of_parse_phandle(parent_np, "zte,lan-pcs-handle", 0);
	if (!pcs_np) {
		ret = -ENODEV;
		dev_err(dev, "LAN PCS phandle is missing\n");
		goto err_power_off;
	}

	priv->xpcs_mdiodev = fwnode_mdio_find_device(of_fwnode_handle(pcs_np));
	of_node_put(pcs_np);
	if (!priv->xpcs_mdiodev) {
		ret = -EPROBE_DEFER;
		dev_err(dev, "XPCS0 MDIO device is unavailable\n");
		goto err_power_off;
	}

	ret = pm_runtime_resume_and_get(priv->xpcs_mdiodev->bus->parent);
	if (ret < 0) {
		mdio_device_put(priv->xpcs_mdiodev);
		dev_err(dev, "failed to enable XPCS0 CSR clock: %d\n", ret);
		goto err_power_off;
	}
	priv->xpcs_runtime_held = true;
	platform_set_drvdata(pdev, priv);

	ret = zx279133_lan_xpcs_configure(priv);
	if (ret) {
		dev_err(dev, "failed to configure XPCS0 USXGMII mode: %d\n", ret);
		goto err_xpcs_runtime;
	}

	zx279133_lan_xmac_configure(priv);
	ret = zx279133_lan_datapath_enable(priv);
	if (ret) {
		dev_err(dev, "failed to enable XMAC0 datapath: %d\n", ret);
		goto err_xmac_restore;
	}
	usleep_range(10000, 11000);
	if (!priv->switch_mdiodev) {
		ret = -ENODEV;
		dev_err(dev, "RTL8372N MDIO device is unavailable\n");
		goto err_datapath_restore;
	}

	ret = rtl8372n_minimal_core_init(dev, priv);
	if (ret)
		goto err_datapath_restore;
	msleep(5000);

	if (!priv->switch_initialized) {
		dev_err(dev, "RTL8372N core initialization incomplete\n");
		ret = -EINVAL;
		goto err_datapath_restore;
	}
	mutex_lock(&priv->switch_mdiodev->bus->mdio_lock);
	ret = rtl8372n_phy_write(priv->switch_mdiodev, BIT(7),
				 0x1f, 0xa5d0, 0);
	mutex_unlock(&priv->switch_mdiodev->bus->mdio_lock);
	if (ret)
		goto err_datapath_restore;
	dev_dbg(dev, "RTL8372N PHY7 page31:a5d0 set to 0\n");

	ret = rtl8372n_cpu8_link_init(dev, priv);
	if (ret)
		goto err_datapath_restore;
	msleep(1000);
	ret = zx279133_lan_xpcs_reapply_after_switch(priv);
	if (ret) {
		dev_err(dev, "XPCS0 did not relock after switch setup: %d\n", ret);
		goto err_datapath_restore;
	}
	msleep(1000);

	ret = rtl8372n_port7_vlan_init(dev, priv);
	if (ret)
		goto err_datapath_restore;
	zx279133_lan_service_datapath_set_ready(priv->service, true);
	priv->parent_datapath_ready = true;
	ret = zx279133_rtl8372n_dsa_register(dev, priv);
	if (ret)
		goto err_datapath_restore;
	zx279133_lan_service_set_dsa_active(priv->service, true);
	msleep(2000);
	dev_info(dev, "XMAC0/RTL8372N DSA datapath enabled\n");

	return 0;

err_datapath_restore:
	if (priv->parent_datapath_ready) {
		int quiesce_ret;

		zx279133_lan_service_datapath_set_ready(priv->service, false);
		priv->parent_datapath_ready = false;
		quiesce_ret =
			zx279133_lan_service_datapath_quiesce(priv->service);
		if (quiesce_ret)
			dev_err(dev,
				"LAN probe unwind is not quiescent; forcing hardware reset and cleanup: %d\n",
				quiesce_ret);
	}
	if (priv->vlan62_tx_active) {
		zx279133_lan_service_set_vlan62_active(priv->service, false);
		priv->vlan62_tx_active = false;
	}
	zx279133_lan_datapath_restore(priv);
err_xmac_restore:
	zx279133_lan_xmac_restore(priv);
	zx279133_lan_xpcs_restore(priv);

err_xpcs_runtime:

	pm_runtime_put(priv->xpcs_mdiodev->bus->parent);
	priv->xpcs_runtime_held = false;
	mdio_device_put(priv->xpcs_mdiodev);
	if (priv->switch_mdiodev) {
		mdio_device_put(priv->switch_mdiodev);
		priv->switch_mdiodev = NULL;
	}
err_power_off:
	if (priv->serdes_powered) {
		phy_power_off(priv->serdes);
		priv->serdes_powered = false;
	}
err_switch_put:
	if (priv->reset_gpio) {
		rtl8372n_reset_assert(priv);
		msleep(RTL8372N_RESET_ASSERT_MS);
	}
	if (priv->switch_mdiodev) {
		mdio_device_put(priv->switch_mdiodev);
		priv->switch_mdiodev = NULL;
	}
err_parent_datapath_put:
	if (priv->parent_datapath_held) {
		zx279133_lan_service_datapath_put(priv->service);
		priv->parent_datapath_held = false;
	}
	return ret;
}

static void zx279133_rtl8372n_remove(struct platform_device *pdev)
{
	struct zx279133_rtl8372n *priv = platform_get_drvdata(pdev);
	int ret;

	if (priv->parent_datapath_ready) {
		zx279133_lan_service_datapath_set_ready(priv->service, false);
		priv->parent_datapath_ready = false;
	}
	zx279133_lan_service_set_dsa_active(priv->service, false);
	synchronize_net();
	if (priv->dsa_registered) {
		dsa_unregister_switch(priv->ds);
		priv->dsa_registered = false;
	}
	if (priv->parent_datapath_held) {
		ret = zx279133_lan_service_datapath_quiesce(priv->service);
		if (ret)
			dev_err(&pdev->dev,
				"LAN teardown is not quiescent; forcing hardware reset and cleanup: %d\n",
				ret);
	}
	if (priv->vlan62_tx_active) {
		zx279133_lan_service_set_vlan62_active(priv->service, false);
		priv->vlan62_tx_active = false;
	}
	rtl8372n_reset_assert(priv);
	msleep(RTL8372N_RESET_ASSERT_MS);
	dev_info(&pdev->dev, "RTL8372N hardware reset asserted\n");
	zx279133_lan_datapath_restore(priv);
	zx279133_lan_xmac_restore(priv);
	zx279133_lan_xpcs_restore(priv);
	if (priv->xpcs_runtime_held)
		pm_runtime_put(priv->xpcs_mdiodev->bus->parent);
	mdio_device_put(priv->xpcs_mdiodev);
	if (priv->switch_mdiodev)
		mdio_device_put(priv->switch_mdiodev);
	if (priv->serdes_powered)
		phy_power_off(priv->serdes);
	if (priv->parent_datapath_held) {
		zx279133_lan_service_datapath_put(priv->service);
		priv->parent_datapath_held = false;
	}
}

static const struct of_device_id zx279133_rtl8372n_of_match[] = {
	{ .compatible = "zte,zx279133-rtl8372n" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_rtl8372n_of_match);

static struct platform_driver zx279133_rtl8372n_driver = {
	.probe = zx279133_rtl8372n_probe,
	.remove = zx279133_rtl8372n_remove,
	.driver = {
		.name = "zx279133-rtl8372n",
		.of_match_table = zx279133_rtl8372n_of_match,
	},
};
module_platform_driver(zx279133_rtl8372n_driver);

MODULE_DESCRIPTION("ZTE ZX279133 RTL8372N DSA switch driver");
MODULE_LICENSE("GPL");
