// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#define ZX279133_MDIO_WRITE_DATA	0x04
#define ZX279133_MDIO_READ_DATA		0x08
#define ZX279133_MDIO_C45_REG		0x0c
#define ZX279133_MDIO_STATUS		0x10
#define ZX279133_MDIO_CONTROL		0x14

#define ZX279133_MDIO_STATUS_DONE	BIT(0)

#define ZX279133_MDIO_CONTROL_REG	GENMASK(4, 0)
#define ZX279133_MDIO_CONTROL_PHY	GENMASK(9, 5)
#define ZX279133_MDIO_CONTROL_OP	GENMASK(11, 10)
#define ZX279133_MDIO_CONTROL_C22	BIT(12)
#define ZX279133_MDIO_CONTROL_C45	BIT(13)
#define ZX279133_MDIO_CONTROL_START	BIT(14)

#define ZX279133_MDIO_OP_C45_ADDR	0
#define ZX279133_MDIO_OP_WRITE		1
#define ZX279133_MDIO_OP_C22_READ	2
#define ZX279133_MDIO_OP_C45_READ	3

#define ZX279133_MDIO_POLL_US		1
#define ZX279133_MDIO_TIMEOUT_US	10000

struct zx279133_mdio {
	void __iomem *base;
};

static int zx279133_mdio_transfer(struct zx279133_mdio *priv, u32 control)
{
	u32 status;
	int ret;

	writel(0, priv->base + ZX279133_MDIO_STATUS);
	writel(control | ZX279133_MDIO_CONTROL_START,
	       priv->base + ZX279133_MDIO_CONTROL);

	ret = readl_poll_timeout(priv->base + ZX279133_MDIO_STATUS, status,
				 status & ZX279133_MDIO_STATUS_DONE,
				 ZX279133_MDIO_POLL_US,
				 ZX279133_MDIO_TIMEOUT_US);

	writel(0, priv->base + ZX279133_MDIO_STATUS);
	writel(control, priv->base + ZX279133_MDIO_CONTROL);

	return ret;
}

static int zx279133_mdio_c22_control(int addr, int regnum, u32 op, u32 *control)
{
	if (addr < 0 || addr > 0x1f || regnum < 0 || regnum > 0x1f)
		return -EINVAL;

	*control = ZX279133_MDIO_CONTROL_C22 |
		   FIELD_PREP(ZX279133_MDIO_CONTROL_OP, op) |
		   FIELD_PREP(ZX279133_MDIO_CONTROL_PHY, addr) |
		   FIELD_PREP(ZX279133_MDIO_CONTROL_REG, regnum);

	return 0;
}

static int zx279133_mdio_read(struct mii_bus *bus, int addr, int regnum)
{
	struct zx279133_mdio *priv = bus->priv;
	u32 control;
	int ret;

	ret = zx279133_mdio_c22_control(addr, regnum,
					ZX279133_MDIO_OP_C22_READ, &control);
	if (ret)
		return ret;

	ret = zx279133_mdio_transfer(priv, control);
	if (ret)
		return ret;

	return readl(priv->base + ZX279133_MDIO_READ_DATA) & 0xffff;
}

static int zx279133_mdio_write(struct mii_bus *bus, int addr, int regnum,
			       u16 value)
{
	struct zx279133_mdio *priv = bus->priv;
	u32 control;
	int ret;

	ret = zx279133_mdio_c22_control(addr, regnum, ZX279133_MDIO_OP_WRITE,
					&control);
	if (ret)
		return ret;

	writel(value, priv->base + ZX279133_MDIO_WRITE_DATA);

	return zx279133_mdio_transfer(priv, control);
}

static int zx279133_mdio_c45_control(int addr, int devnum, u32 *control)
{
	if (addr < 0 || addr > 0x1f || devnum < 0 || devnum > 0x1f)
		return -EINVAL;

	*control = ZX279133_MDIO_CONTROL_C45 |
		   FIELD_PREP(ZX279133_MDIO_CONTROL_PHY, addr) |
		   FIELD_PREP(ZX279133_MDIO_CONTROL_REG, devnum);

	return 0;
}

static int zx279133_mdio_c45_address(struct zx279133_mdio *priv, u32 control,
				     int regnum)
{
	if (regnum < 0 || regnum > 0xffff)
		return -EINVAL;

	writel(regnum, priv->base + ZX279133_MDIO_C45_REG);

	return zx279133_mdio_transfer(priv,
		control | FIELD_PREP(ZX279133_MDIO_CONTROL_OP,
				     ZX279133_MDIO_OP_C45_ADDR));
}

static int zx279133_mdio_read_c45(struct mii_bus *bus, int addr, int devnum,
				  int regnum)
{
	struct zx279133_mdio *priv = bus->priv;
	u32 control;
	int ret;

	ret = zx279133_mdio_c45_control(addr, devnum, &control);
	if (ret)
		return ret;

	ret = zx279133_mdio_c45_address(priv, control, regnum);
	if (ret)
		return ret;

	control |= FIELD_PREP(ZX279133_MDIO_CONTROL_OP,
			      ZX279133_MDIO_OP_C45_READ);
	ret = zx279133_mdio_transfer(priv, control);
	if (ret)
		return ret;

	return readl(priv->base + ZX279133_MDIO_READ_DATA) & 0xffff;
}

static int zx279133_mdio_write_c45(struct mii_bus *bus, int addr, int devnum,
				   int regnum, u16 value)
{
	struct zx279133_mdio *priv = bus->priv;
	u32 control;
	int ret;

	ret = zx279133_mdio_c45_control(addr, devnum, &control);
	if (ret)
		return ret;

	ret = zx279133_mdio_c45_address(priv, control, regnum);
	if (ret)
		return ret;

	writel(value, priv->base + ZX279133_MDIO_WRITE_DATA);

	control |= FIELD_PREP(ZX279133_MDIO_CONTROL_OP,
			      ZX279133_MDIO_OP_WRITE);

	return zx279133_mdio_transfer(priv, control);
}

static int zx279133_mdio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_mdio *priv;
	struct mii_bus *bus;
	struct reset_control *reset;
	struct clk *pclk;
	struct clk *wclk;
	int ret;

	bus = devm_mdiobus_alloc_size(dev, sizeof(*priv));
	if (!bus)
		return -ENOMEM;

	priv = bus->priv;
	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	pclk = devm_clk_get_enabled(dev, "pclk");
	if (IS_ERR(pclk))
		return dev_err_probe(dev, PTR_ERR(pclk),
				     "failed to enable peripheral clock\n");

	wclk = devm_clk_get_enabled(dev, "wclk");
	if (IS_ERR(wclk))
		return dev_err_probe(dev, PTR_ERR(wclk),
				     "failed to enable working clock\n");

	reset = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(reset))
		return dev_err_probe(dev, PTR_ERR(reset),
				     "failed to get controller reset\n");

	ret = reset_control_reset(reset);
	if (ret)
		return dev_err_probe(dev, ret, "failed to reset controller\n");

	bus->name = "ZX279133 MDIO";
	bus->parent = dev;
	bus->read = zx279133_mdio_read;
	bus->write = zx279133_mdio_write;
	bus->read_c45 = zx279133_mdio_read_c45;
	bus->write_c45 = zx279133_mdio_write_c45;
	bus->phy_mask = GENMASK(31, 0);
	snprintf(bus->id, MII_BUS_ID_SIZE, "%s", dev_name(dev));

	return devm_of_mdiobus_register(dev, bus, dev->of_node);
}

static const struct of_device_id zx279133_mdio_of_match[] = {
	{ .compatible = "zte,zx279133-mdio" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_mdio_of_match);

static struct platform_driver zx279133_mdio_driver = {
	.probe = zx279133_mdio_probe,
	.driver = {
		.name = "zx279133-mdio",
		.of_match_table = zx279133_mdio_of_match,
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(zx279133_mdio_driver);

MODULE_DESCRIPTION("ZTE ZX279133 MDIO bus controller");
MODULE_LICENSE("GPL");
