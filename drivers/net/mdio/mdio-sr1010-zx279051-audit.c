// SPDX-License-Identifier: GPL-2.0-only

#include <linux/atomic.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/phy.h>

#define SR1010_ZX279051_MDIO_BASE	0x14f02000
#define SR1010_ZX279051_ADDR		1
#define SR1010_ZX279051_ID_DEVAD		7
#define SR1010_ZX279051_ID_REG		2
#define SR1010_ZX279051_ID_EXPECTED	0x84b9

static atomic_t sr1010_zx279051_audit_attempted = ATOMIC_INIT(0);

static int sr1010_zx279051_audit_probe(struct mdio_device *mdiodev)
{
	struct resource resource;
	int first;
	int second;

	if (mdiodev->addr != SR1010_ZX279051_ADDR)
		return -EINVAL;
	if (!mdiodev->bus->parent ||
	    of_address_to_resource(mdiodev->bus->parent->of_node, 0, &resource) ||
	    resource.start != SR1010_ZX279051_MDIO_BASE)
		return -EINVAL;
	if (atomic_xchg(&sr1010_zx279051_audit_attempted, 1))
		return -EBUSY;

	first = mdiobus_c45_read(mdiodev->bus, mdiodev->addr,
				 SR1010_ZX279051_ID_DEVAD,
				 SR1010_ZX279051_ID_REG);
	second = mdiobus_c45_read(mdiodev->bus, mdiodev->addr,
				  SR1010_ZX279051_ID_DEVAD,
				  SR1010_ZX279051_ID_REG);

	if (first < 0 || second < 0) {
		dev_err(&mdiodev->dev, "read failed: first=%d second=%d\n",
			first, second);
		return first < 0 ? first : second;
	}

	if (first != SR1010_ZX279051_ID_EXPECTED || first != second) {
		dev_err(&mdiodev->dev,
			"unexpected ID: first=%#x second=%#x expected=%#x\n",
			first, second, SR1010_ZX279051_ID_EXPECTED);
		return -ENODEV;
	}

	dev_info(&mdiodev->dev,
		 "read-only ID devad=%u reg=%u first=%#x second=%#x\n",
		 SR1010_ZX279051_ID_DEVAD, SR1010_ZX279051_ID_REG,
		 first, second);

	return 0;
}

static const struct of_device_id sr1010_zx279051_audit_of_match[] = {
	{ .compatible = "zte,sr1010-zx279051-audit" },
	{ }
};
MODULE_DEVICE_TABLE(of, sr1010_zx279051_audit_of_match);

static struct mdio_driver sr1010_zx279051_audit_driver = {
	.mdiodrv.driver = {
		.name = "sr1010-zx279051-audit",
		.of_match_table = sr1010_zx279051_audit_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = sr1010_zx279051_audit_probe,
};

mdio_module_driver(sr1010_zx279051_audit_driver);

MODULE_DESCRIPTION("ZTE SR1010 ZX279051 read-only identification audit");
MODULE_LICENSE("GPL");
