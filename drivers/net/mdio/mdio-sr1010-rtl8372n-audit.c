// SPDX-License-Identifier: GPL-2.0-only

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

#define SR1010_RTL8372N_ID_REG		0x0004
#define SR1010_RTL8372N_READ_CMD		0x001b
#define SR1010_RTL8372N_ID_PREFIX	0x837270
#define SR1010_RTL8372N_POLL_MAX		32

struct sr1010_rtl8372n_counts {
	unsigned int reads;
	unsigned int writes;
	unsigned int polls[4];
};

static atomic_t sr1010_rtl8372n_audit_attempted = ATOMIC_INIT(0);

static int rtl8372n_read(struct mdio_device *mdiodev, u32 reg,
			 struct sr1010_rtl8372n_counts *counts)
{
	counts->reads++;
	return __mdiodev_read(mdiodev, reg);
}

static int rtl8372n_write(struct mdio_device *mdiodev, u32 reg, u16 value,
			  struct sr1010_rtl8372n_counts *counts)
{
	counts->writes++;
	return __mdiodev_write(mdiodev, reg, value);
}

static int sr1010_rtl8372n_wait_ready(struct mdio_device *mdiodev,
				      struct sr1010_rtl8372n_counts *counts,
				      unsigned int poll)
{
	unsigned int i;
	int value;

	for (i = 0; i < SR1010_RTL8372N_POLL_MAX; i++) {
		counts->polls[poll]++;
		value = rtl8372n_read(mdiodev, SR1010_RTL8372N_SMI_CTRL, counts);
		if (value < 0)
			return value;
		if (!(value & SR1010_RTL8372N_SMI_BUSY))
			return 0;
		if (i + 1 < SR1010_RTL8372N_POLL_MAX)
			usleep_range(100, 200);
	}

	return -ETIMEDOUT;
}

static int sr1010_rtl8372n_read_id(struct mdio_device *mdiodev,
				   struct sr1010_rtl8372n_counts *counts,
				   unsigned int first_poll, u32 *id)
{
	int high;
	int low;
	int ret;

	ret = sr1010_rtl8372n_wait_ready(mdiodev, counts, first_poll);
	if (ret)
		return ret;

	ret = rtl8372n_write(mdiodev, SR1010_RTL8372N_SMI_ADDR,
			     SR1010_RTL8372N_ID_REG, counts);
	if (ret)
		return ret;

	ret = rtl8372n_write(mdiodev, SR1010_RTL8372N_SMI_CTRL,
			     SR1010_RTL8372N_READ_CMD, counts);
	if (ret)
		return ret;

	ret = sr1010_rtl8372n_wait_ready(mdiodev, counts, first_poll + 1);
	if (ret)
		return ret;

	low = rtl8372n_read(mdiodev, SR1010_RTL8372N_SMI_DATA_LOW, counts);
	if (low < 0)
		return low;

	high = rtl8372n_read(mdiodev, SR1010_RTL8372N_SMI_DATA_HIGH, counts);
	if (high < 0)
		return high;

	*id = (u32)high << 16 | low;

	return 0;
}

static int sr1010_rtl8372n_audit_probe(struct mdio_device *mdiodev)
{
	struct sr1010_rtl8372n_counts counts = { };
	struct device *parent = mdiodev->bus->parent;
	struct resource resource;
	u32 first;
	u32 second;
	int ret;

	if (mdiodev->addr != SR1010_RTL8372N_ADDR || !parent ||
	    !of_device_is_compatible(parent->of_node, "zte,zx279133-mdio") ||
	    of_address_to_resource(parent->of_node, 0, &resource) ||
	    resource.start != SR1010_RTL8372N_MDIO_BASE ||
	    resource_size(&resource) != SR1010_RTL8372N_MDIO_SIZE)
		return -EINVAL;

	if (atomic_xchg(&sr1010_rtl8372n_audit_attempted, 1))
		return -EBUSY;

	mutex_lock(&mdiodev->bus->mdio_lock);
	ret = sr1010_rtl8372n_read_id(mdiodev, &counts, 0, &first);
	if (!ret)
		ret = sr1010_rtl8372n_read_id(mdiodev, &counts, 2, &second);
	mutex_unlock(&mdiodev->bus->mdio_lock);
	if (ret) {
		dev_err(&mdiodev->dev, "chip ID read failed: %d\n", ret);
		return ret;
	}

	if (first != second || (first >> 8) != SR1010_RTL8372N_ID_PREFIX) {
		dev_err(&mdiodev->dev,
			"unexpected chip ID first=0x%08x second=0x%08x\n",
			first, second);
		return -ENODEV;
	}

	dev_info(&mdiodev->dev,
		 "read-only chip ID reg=0x4 first=0x%08x second=0x%08x reads=%u writes=%u polls=%u,%u,%u,%u\n",
		 first, second, counts.reads, counts.writes, counts.polls[0],
		 counts.polls[1], counts.polls[2], counts.polls[3]);

	return 0;
}

static const struct of_device_id sr1010_rtl8372n_audit_of_match[] = {
	{ .compatible = "zte,sr1010-rtl8372n-id-audit" },
	{ }
};
MODULE_DEVICE_TABLE(of, sr1010_rtl8372n_audit_of_match);

static struct mdio_driver sr1010_rtl8372n_audit_driver = {
	.mdiodrv.driver = {
		.name = "sr1010-rtl8372n-id-audit",
		.of_match_table = sr1010_rtl8372n_audit_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = sr1010_rtl8372n_audit_probe,
};

mdio_module_driver(sr1010_rtl8372n_audit_driver);

MODULE_DESCRIPTION("ZTE SR1010 RTL8372N read-only chip ID audit");
MODULE_LICENSE("GPL");
