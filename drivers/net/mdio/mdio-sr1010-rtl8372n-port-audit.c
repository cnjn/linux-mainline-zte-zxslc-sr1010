// SPDX-License-Identifier: GPL-2.0-only
/*
 * ZTE SR1010 RTL8372N read-only port status audit.
 *
 * This is a bounded bring-up diagnostic. It performs no switch
 * configuration writes; the only writes are the RTL8372N SMI address and
 * command registers required to issue read transactions.
 */

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/phy.h>

#define SR1010_RTL8372N_MDIO_BASE	0x14f01000
#define SR1010_RTL8372N_MDIO_SIZE	0x1000
#define SR1010_RTL8372N_ADDR		0x1d

#define SR1010_RTL8372N_SMI_CTRL		0x15
#define SR1010_RTL8372N_SMI_BUSY		BIT(2)
#define SR1010_RTL8372N_SMI_ADDR		0x16
#define SR1010_RTL8372N_SMI_DATA_LOW	0x17
#define SR1010_RTL8372N_SMI_DATA_HIGH	0x18
#define SR1010_RTL8372N_READ_CMD		0x001b
#define SR1010_RTL8372N_POLL_MAX		32
#define SR1010_RTL8372N_ID_REG		0x0004
#define SR1010_RTL8372N_ID_PREFIX		0x837270

#define SR1010_RTL8372N_LINK_STATUS	0x63e8
#define SR1010_RTL8372N_MEDIA_STATUS	0x63ec
#define SR1010_RTL8372N_SPEED_STATUS0	0x63f0
#define SR1010_RTL8372N_SPEED_STATUS1	0x63f4
#define SR1010_RTL8372N_DUPLEX_STATUS	0x63f8

static atomic_t sr1010_rtl8372n_status_attempted = ATOMIC_INIT(0);

static int sr1010_rtl8372n_wait_ready(struct mdio_device *mdiodev)
{
	int value;
	unsigned int i;

	for (i = 0; i < SR1010_RTL8372N_POLL_MAX; i++) {
		value = __mdiodev_read(mdiodev, SR1010_RTL8372N_SMI_CTRL);
		if (value < 0)
			return value;
		if (!(value & SR1010_RTL8372N_SMI_BUSY))
			return 0;
		if (i + 1 < SR1010_RTL8372N_POLL_MAX)
			usleep_range(100, 200);
	}

	return -ETIMEDOUT;
}

static int sr1010_rtl8372n_read_reg(struct mdio_device *mdiodev, u16 reg,
					    u32 *value)
{
	int low, high, ret;

	ret = sr1010_rtl8372n_wait_ready(mdiodev);
	if (ret)
		return ret;

	ret = __mdiodev_write(mdiodev, SR1010_RTL8372N_SMI_ADDR, reg);
	if (ret)
		return ret;
	ret = __mdiodev_write(mdiodev, SR1010_RTL8372N_SMI_CTRL,
				      SR1010_RTL8372N_READ_CMD);
	if (ret)
		return ret;

	ret = sr1010_rtl8372n_wait_ready(mdiodev);
	if (ret)
		return ret;

	low = __mdiodev_read(mdiodev, SR1010_RTL8372N_SMI_DATA_LOW);
	if (low < 0)
		return low;
	high = __mdiodev_read(mdiodev, SR1010_RTL8372N_SMI_DATA_HIGH);
	if (high < 0)
		return high;

	*value = (u32)low | (u32)high << 16;
	return 0;
}

static int sr1010_rtl8372n_status_probe(struct mdio_device *mdiodev)
{
	struct device *parent = mdiodev->bus->parent;
	struct resource resource;
	u32 id1, id2, link, media, speed0, speed1, duplex;
	unsigned int port;
	int ret;

	if (mdiodev->addr != SR1010_RTL8372N_ADDR || !parent ||
	    !of_device_is_compatible(parent->of_node, "zte,zx279133-mdio") ||
	    of_address_to_resource(parent->of_node, 0, &resource) ||
	    resource.start != SR1010_RTL8372N_MDIO_BASE ||
	    resource_size(&resource) != SR1010_RTL8372N_MDIO_SIZE)
		return -EINVAL;

	if (atomic_xchg(&sr1010_rtl8372n_status_attempted, 1))
		return -EBUSY;

	mutex_lock(&mdiodev->bus->mdio_lock);
	ret = sr1010_rtl8372n_read_reg(mdiodev, SR1010_RTL8372N_ID_REG, &id1);
	if (!ret)
		ret = sr1010_rtl8372n_read_reg(mdiodev, SR1010_RTL8372N_ID_REG, &id2);
	if (!ret && (id1 != id2 || (id1 >> 8) != SR1010_RTL8372N_ID_PREFIX))
		ret = -ENODEV;
	if (!ret)
		ret = sr1010_rtl8372n_read_reg(mdiodev, SR1010_RTL8372N_LINK_STATUS,
					&link);
	if (!ret)
		ret = sr1010_rtl8372n_read_reg(mdiodev,
					SR1010_RTL8372N_MEDIA_STATUS, &media);
	if (!ret)
		ret = sr1010_rtl8372n_read_reg(mdiodev,
					SR1010_RTL8372N_SPEED_STATUS0, &speed0);
	if (!ret)
		ret = sr1010_rtl8372n_read_reg(mdiodev,
					SR1010_RTL8372N_SPEED_STATUS1, &speed1);
	if (!ret)
		ret = sr1010_rtl8372n_read_reg(mdiodev,
					SR1010_RTL8372N_DUPLEX_STATUS, &duplex);
	mutex_unlock(&mdiodev->bus->mdio_lock);
	if (ret) {
		dev_err(&mdiodev->dev, "port status read failed: %d\n", ret);
		return ret;
	}

	dev_info(&mdiodev->dev,
		 "read-only chip ID first=%#010x second=%#010x\n", id1, id2);
	dev_info(&mdiodev->dev,
		 "read-only port status raw link=%#010x media=%#010x speed0=%#010x speed1=%#010x duplex=%#010x\n",
		 link, media, speed0, speed1, duplex);

	for (port = 3; port <= 8; port++) {
		u32 speed = port < 8 ? speed0 >> ((port & 7) * 4) : speed1;

		dev_info(&mdiodev->dev,
			 "port%u link=%u speed_code=%u duplex=%u media=%u\n",
			 port, !!(link & BIT(port)), speed & 0xf,
			 !!(duplex & BIT(port)), !!(media & BIT(port)));
	}

	return 0;
}

static const struct of_device_id sr1010_rtl8372n_status_of_match[] = {
	{ .compatible = "zte,sr1010-rtl8372n-port-status-audit" },
	{ }
};
MODULE_DEVICE_TABLE(of, sr1010_rtl8372n_status_of_match);

static struct mdio_driver sr1010_rtl8372n_status_driver = {
	.mdiodrv.driver = {
		.name = "sr1010-rtl8372n-port-status-audit",
		.of_match_table = sr1010_rtl8372n_status_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = sr1010_rtl8372n_status_probe,
};

mdio_module_driver(sr1010_rtl8372n_status_driver);

MODULE_DESCRIPTION("ZTE SR1010 RTL8372N read-only port status audit");
MODULE_LICENSE("GPL");
