// SPDX-License-Identifier: GPL-2.0-only

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>

#include <dt-bindings/clock/zte,zx279133-lsp.h>
#include <dt-bindings/clock/zte,zx279133-topcrm.h>
#include <dt-bindings/reset/zte,zx279133-lsp.h>

#define ZX279133_TOPCRM_GATE_CTRL	0x38
#define ZX279133_TOPCRM_GATE_FLAGS	CLK_IGNORE_UNUSED
#define ZX279133_UART0_CLK_CTRL	0x24
#define ZX279133_UART_PCLK_GATE	0
#define ZX279133_UART_WCLK_GATE	1
#define ZX279133_UART_WCLK_MUX	9
#define ZX279133_UART_RESET	4
#define ZX279133_UART_CLK_FLAGS	(CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED)
#define ZX279133_UART_WCLK_FLAGS	(ZX279133_UART_CLK_FLAGS | CLK_IS_CRITICAL)
#define ZX279133_WDT0_CLK_CTRL	0x34
#define ZX279133_WDT1_CLK_CTRL	0x38
#define ZX279133_WDT_PCLK_GATE	0
#define ZX279133_WDT_WCLK_GATE	1
#define ZX279133_WDT_WCLK_DIV_SHIFT	11
#define ZX279133_WDT_WCLK_DIV_WIDTH	10
#define ZX279133_WDT_CLK_FLAGS	CLK_IGNORE_UNUSED
#define ZX279133_TOPCRM_PVT_DIV_CTRL	0x58
#define ZX279133_TOPCRM_PVT_GATE_CTRL	0x40
#define ZX279133_TOPCRM_SPIFC_GATE_CTRL	0x38
#define ZX279133_TOPCRM_SPIFC_GATE_BIT	16
#define ZX279133_TOPCRM_USB_GATE_CTRL	0x3c
#define ZX279133_TOPCRM_USB_GATE_FLAGS	0
#define ZX279133_LSP_SPIFC_CLK_CTRL	0x2c
#define ZX279133_LSP_SPIFC_PCLK_GATE	0
#define ZX279133_LSP_SPIFC_WCLK_GATE	1
#define ZX279133_LSP_SPIFC_WCLK_DIV_SHIFT	6
#define ZX279133_LSP_SPIFC_WCLK_DIV_WIDTH	6
#define ZX279133_LSP_SPIFC_RESET	4

struct zx279133_topcrm_gate_desc {
	const char *name;
	const struct clk_parent_data *parent;
	unsigned long flags;
	u16 reg_offset;
	u8 bit_idx;
};

struct zx279133_topcrm_clk {
	spinlock_t lock; /* Protects the shared TOPCRM gate register. */
	struct clk_hw_onecell_data data;
};

struct zx279133_lsp_clk {
	spinlock_t lock; /* Protects the LSP clock control registers. */
	struct clk_hw_onecell_data *data;
	struct clk_mux uart0_wclk_mux;
	struct clk_gate uart0_wclk_gate;
	struct clk_divider spifc_wclk_divider;
	struct clk_gate spifc_wclk_gate;
	struct reset_controller_dev rcdev;
	void __iomem *uart0_reg;
	void __iomem *spifc_reg;
};

struct zx279133_lsp1_clk {
	spinlock_t lock; /* Protects the shared watchdog clock registers. */
	struct clk_divider wdt_dividers[2];
	struct clk_gate wdt_gates[2];
	struct clk_hw_onecell_data data;
};

static const struct clk_parent_data topcrm_clk100m_parent = {
	.fw_name = "clk100m",
};

static const struct clk_parent_data topcrm_clk25m_parent = {
	.fw_name = "clk25m",
};

static const struct clk_parent_data topcrm_pclk_parent = {
	.fw_name = "pclk",
};

static const struct clk_parent_data topcrm_sys_pclk_parent = {
	.fw_name = "sys_pclk",
};

static const struct clk_parent_data topcrm_cci_aclk_parent = {
	.fw_name = "cci_aclk",
};

static const struct clk_parent_data topcrm_sys_aclk_parent = {
	.fw_name = "sys_aclk",
};

static const struct clk_parent_data topcrm_clk32k_parent = {
	.fw_name = "clk32k",
};

static const struct zx279133_topcrm_gate_desc topcrm_gates[] = {
	[ZX279133_TOPCRM_CLK_LSP0_100M] = {
		.name = "lsp0_100m",
		.parent = &topcrm_clk100m_parent,
		.flags = ZX279133_TOPCRM_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 12,
	},
	[ZX279133_TOPCRM_CLK_LSP0_25M] = {
		.name = "lsp0_25m",
		.parent = &topcrm_clk25m_parent,
		.flags = ZX279133_TOPCRM_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 13,
	},
	[ZX279133_TOPCRM_CLK_LSP0_PCLK] = {
		.name = "lsp0_pclk",
		.parent = &topcrm_pclk_parent,
		.flags = ZX279133_TOPCRM_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 15,
	},
	[ZX279133_TOPCRM_CLK_LSP1_32K] = {
		.name = "lsp1_32k",
		.parent = &topcrm_clk32k_parent,
		.flags = ZX279133_TOPCRM_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 21,
	},
	[ZX279133_TOPCRM_CLK_LSP1_PCLK] = {
		.name = "lsp1_pclk",
		.parent = &topcrm_pclk_parent,
		.flags = ZX279133_TOPCRM_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 22,
	},
	[ZX279133_TOPCRM_CLK_TEMPSENSOR_WCLK] = {
		.name = "tempsensor_wclk",
		.flags = 0,
		.reg_offset = ZX279133_TOPCRM_PVT_GATE_CTRL,
		.bit_idx = 0,
	},
	[ZX279133_TOPCRM_CLK_SPIFC_WCLK] = {
		.name = "spifc_wclk",
		.parent = &topcrm_clk100m_parent,
		.flags = 0,
		.reg_offset = ZX279133_TOPCRM_SPIFC_GATE_CTRL,
		.bit_idx = ZX279133_TOPCRM_SPIFC_GATE_BIT,
	},
	[ZX279133_TOPCRM_CLK_USB_PCLK] = {
		.name = "usb_pclk",
		.parent = &topcrm_sys_pclk_parent,
		.flags = ZX279133_TOPCRM_USB_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_USB_GATE_CTRL,
		.bit_idx = 4,
	},
	[ZX279133_TOPCRM_CLK_USB_CCI_ACLK] = {
		.name = "usb_cci_aclk",
		.parent = &topcrm_cci_aclk_parent,
		.flags = ZX279133_TOPCRM_USB_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_USB_GATE_CTRL,
		.bit_idx = 5,
	},
	[ZX279133_TOPCRM_CLK_USB_ACLK] = {
		.name = "usb_aclk",
		.parent = &topcrm_sys_aclk_parent,
		.flags = ZX279133_TOPCRM_USB_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_USB_GATE_CTRL,
		.bit_idx = 6,
	},
};

static int zx279133_topcrm_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_topcrm_clk *priv;
	struct clk_hw *pvt_div;
	void __iomem *base;
	void __iomem *reg;
	unsigned int index;
	int ret;

	priv = devm_kzalloc(dev, struct_size(priv, data.hws,
					     ARRAY_SIZE(topcrm_gates)), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	spin_lock_init(&priv->lock);
	pvt_div = devm_clk_hw_register_divider(dev, "clk1m22", "clk50m",
					       0, base + ZX279133_TOPCRM_PVT_DIV_CTRL,
					      0, 6, CLK_DIVIDER_READ_ONLY,
					      &priv->lock);
	if (IS_ERR(pvt_div))
		return dev_err_probe(dev, PTR_ERR(pvt_div),
				     "failed to register clk1m22 divider\n");

	for (index = 0; index < ARRAY_SIZE(topcrm_gates); index++) {
		const struct zx279133_topcrm_gate_desc *desc =
			&topcrm_gates[index];
		struct clk_hw *hw;

		reg = base + desc->reg_offset;
		if (index == ZX279133_TOPCRM_CLK_TEMPSENSOR_WCLK)
			hw = devm_clk_hw_register_gate_parent_hw(dev, desc->name,
								 pvt_div, desc->flags,
								 reg, desc->bit_idx, 0,
								 &priv->lock);
		else
			hw = devm_clk_hw_register_gate_parent_data(dev, desc->name,
								   desc->parent,
								   desc->flags, reg,
								   desc->bit_idx, 0,
								   &priv->lock);
		if (IS_ERR(hw))
			return dev_err_probe(dev, PTR_ERR(hw),
					     "failed to register %s gate\n",
					     desc->name);

		priv->data.hws[index] = hw;
	}

	priv->data.num = ARRAY_SIZE(topcrm_gates);
	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					  &priv->data);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to register clock provider\n");

	return 0;
}

static int zx279133_lsp_reset_update(struct reset_controller_dev *rcdev,
				     unsigned long id, bool deassert)
{
	struct zx279133_lsp_clk *priv =
		container_of(rcdev, struct zx279133_lsp_clk, rcdev);
	void __iomem *reg;
	u8 bit;
	unsigned long flags;
	u32 value;

	switch (id) {
	case ZX279133_LSP_RESET_UART0:
		reg = priv->uart0_reg;
		bit = ZX279133_UART_RESET;
		break;
	case ZX279133_LSP_RESET_SPIFC:
		reg = priv->spifc_reg;
		bit = ZX279133_LSP_SPIFC_RESET;
		break;
	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&priv->lock, flags);
	value = readl(reg);
	if (deassert)
		value |= BIT(bit);
	else
		value &= ~BIT(bit);
	writel(value, reg);
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int zx279133_lsp_reset_assert(struct reset_controller_dev *rcdev,
				     unsigned long id)
{
	return zx279133_lsp_reset_update(rcdev, id, false);
}

static int zx279133_lsp_reset_deassert(struct reset_controller_dev *rcdev,
				       unsigned long id)
{
	return zx279133_lsp_reset_update(rcdev, id, true);
}

static int zx279133_lsp_reset_reset(struct reset_controller_dev *rcdev,
				    unsigned long id)
{
	int ret;

	ret = zx279133_lsp_reset_assert(rcdev, id);
	if (ret)
		return ret;

	/* Match the vendor controller's 10 ms reset hold time. */
	usleep_range(10000, 11000);

	return zx279133_lsp_reset_deassert(rcdev, id);
}

static int zx279133_lsp_reset_status(struct reset_controller_dev *rcdev,
				     unsigned long id)
{
	struct zx279133_lsp_clk *priv =
		container_of(rcdev, struct zx279133_lsp_clk, rcdev);
	void __iomem *reg;
	u8 bit;

	switch (id) {
	case ZX279133_LSP_RESET_UART0:
		reg = priv->uart0_reg;
		bit = ZX279133_UART_RESET;
		break;
	case ZX279133_LSP_RESET_SPIFC:
		reg = priv->spifc_reg;
		bit = ZX279133_LSP_SPIFC_RESET;
		break;
	default:
		return -EINVAL;
	}

	return !(readl(reg) & BIT(bit));
}

static const struct reset_control_ops zx279133_lsp_reset_ops = {
	.assert = zx279133_lsp_reset_assert,
	.deassert = zx279133_lsp_reset_deassert,
	.reset = zx279133_lsp_reset_reset,
	.status = zx279133_lsp_reset_status,
};

static const struct clk_parent_data uart0_pclk_parent = {
	.fw_name = "pclk",
};

static const struct clk_parent_data uart0_wclk_parents[] = {
	{ .fw_name = "wclk25" },
	{ .fw_name = "wclk100" },
};

static const struct clk_parent_data spifc_wclk_parent = {
	.fw_name = "spifc_wclk",
};

static const struct clk_parent_data wdt_pclk_parent = {
	.fw_name = "pclk",
};

static const struct clk_parent_data wdt_wclk_parent = {
	.fw_name = "wclk32k",
};

static int zx279133_lsp1_clk_probe(struct platform_device *pdev)
{
	static const unsigned int offsets[] = {
		ZX279133_WDT0_CLK_CTRL, ZX279133_WDT1_CLK_CTRL,
	};
	static const char * const names[][2] = {
		{ "wdt0_pclk", "wdt0_wclk" },
		{ "wdt1_pclk", "wdt1_wclk" },
	};
	struct device *dev = &pdev->dev;
	struct zx279133_lsp1_clk *priv;
	struct clk_hw_onecell_data *data;
	void __iomem *base;
	unsigned int index;
	int ret;

	priv = devm_kzalloc(dev, struct_size(priv, data.hws, 4), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	spin_lock_init(&priv->lock);
	for (index = 0; index < ARRAY_SIZE(offsets); index++) {
		struct clk_divider *divider = &priv->wdt_dividers[index];
		struct clk_gate *gate = &priv->wdt_gates[index];
		struct clk_hw *hw;
		void __iomem *reg = base + offsets[index];

		hw = devm_clk_hw_register_gate_parent_data(dev, names[index][0],
							   &wdt_pclk_parent,
							   ZX279133_WDT_CLK_FLAGS,
							   reg, ZX279133_WDT_PCLK_GATE,
							   0, &priv->lock);
		if (IS_ERR(hw))
			return dev_err_probe(dev, PTR_ERR(hw),
					     "failed to register WDT%u PCLK\n", index);
		priv->data.hws[index * 2] = hw;

		divider->reg = reg;
		divider->shift = ZX279133_WDT_WCLK_DIV_SHIFT;
		divider->width = ZX279133_WDT_WCLK_DIV_WIDTH;
		divider->lock = &priv->lock;

		gate->reg = reg;
		gate->bit_idx = ZX279133_WDT_WCLK_GATE;
		gate->lock = &priv->lock;

		hw = devm_clk_hw_register_composite_pdata(dev, names[index][1],
							  &wdt_wclk_parent, 1,
							  NULL, NULL,
							  &divider->hw,
							  &clk_divider_ops,
							  &gate->hw,
							  &clk_gate_ops,
							  ZX279133_WDT_CLK_FLAGS);
		if (IS_ERR(hw))
			return dev_err_probe(dev, PTR_ERR(hw),
					     "failed to register WDT%u WCLK\n", index);
		priv->data.hws[index * 2 + 1] = hw;
	}

	priv->data.num = 4;
	data = &priv->data;

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, data);
	if (ret)
		return ret;

	dev_info(dev, "registered 4 watchdog clocks\n");

	return 0;
}

static int zx279133_lsp0_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_lsp_clk *priv;
	struct clk_hw *hw;
	void __iomem *base;
	void __iomem *reg;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->data = devm_kzalloc(dev, struct_size(priv->data, hws, 4), GFP_KERNEL);
	if (!priv->data)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	spin_lock_init(&priv->lock);
	reg = base + ZX279133_UART0_CLK_CTRL;
	priv->uart0_reg = reg;

	hw = devm_clk_hw_register_gate_parent_data(dev, "uart0_pclk",
						   &uart0_pclk_parent,
						   ZX279133_UART_CLK_FLAGS, reg,
						   ZX279133_UART_PCLK_GATE, 0,
						   &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
			"failed to register UART0 PCLK\n");
	priv->data->hws[ZX279133_LSP_CLK_UART0_PCLK] = hw;

	priv->uart0_wclk_mux.reg = reg;
	priv->uart0_wclk_mux.shift = ZX279133_UART_WCLK_MUX;
	priv->uart0_wclk_mux.mask = 1;
	priv->uart0_wclk_mux.lock = &priv->lock;

	priv->uart0_wclk_gate.reg = reg;
	priv->uart0_wclk_gate.bit_idx = ZX279133_UART_WCLK_GATE;
	priv->uart0_wclk_gate.lock = &priv->lock;

	hw = devm_clk_hw_register_composite_pdata(dev, "uart0_wclk",
						  uart0_wclk_parents,
						  ARRAY_SIZE(uart0_wclk_parents),
						  &priv->uart0_wclk_mux.hw,
						  &clk_mux_ops, NULL, NULL,
						  &priv->uart0_wclk_gate.hw,
						  &clk_gate_ops,
						  ZX279133_UART_WCLK_FLAGS);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register UART0 WCLK\n");
	priv->data->hws[ZX279133_LSP_CLK_UART0_WCLK] = hw;

	reg = base + ZX279133_LSP_SPIFC_CLK_CTRL;
	priv->spifc_reg = reg;

	hw = devm_clk_hw_register_gate_parent_data(dev, "sfc_pclk",
						   &uart0_pclk_parent, 0, reg,
						   ZX279133_LSP_SPIFC_PCLK_GATE,
						   0, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register SPI-FC PCLK\n");
	priv->data->hws[ZX279133_LSP_CLK_SPIFC_PCLK] = hw;

	priv->spifc_wclk_divider.reg = reg;
	priv->spifc_wclk_divider.shift = ZX279133_LSP_SPIFC_WCLK_DIV_SHIFT;
	priv->spifc_wclk_divider.width = ZX279133_LSP_SPIFC_WCLK_DIV_WIDTH;
	priv->spifc_wclk_divider.lock = &priv->lock;

	priv->spifc_wclk_gate.reg = reg;
	priv->spifc_wclk_gate.bit_idx = ZX279133_LSP_SPIFC_WCLK_GATE;
	priv->spifc_wclk_gate.lock = &priv->lock;

	hw = devm_clk_hw_register_composite_pdata(dev, "sfc_wclk",
						  &spifc_wclk_parent, 1,
						  NULL, NULL,
						  &priv->spifc_wclk_divider.hw,
						  &clk_divider_ops,
						  &priv->spifc_wclk_gate.hw,
						  &clk_gate_ops, 0);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register SPI-FC WCLK\n");
	priv->data->hws[ZX279133_LSP_CLK_SPIFC_WCLK] = hw;

	priv->data->num = 4;
	priv->rcdev.ops = &zx279133_lsp_reset_ops;
	priv->rcdev.owner = THIS_MODULE;
	priv->rcdev.of_node = dev->of_node;
	priv->rcdev.nr_resets = 2;

	ret = devm_reset_controller_register(dev, &priv->rcdev);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to register reset controller\n");

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   priv->data);
}

static const struct of_device_id zx279133_lsp_clk_of_match[] = {
	{ .compatible = "zte,zx279133-topcrm" },
	{ .compatible = "zte,zx279133-lsp-clk" },
	{ .compatible = "zte,zx279133-lsp1-clk" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_lsp_clk_of_match);

static int zx279133_clk_probe(struct platform_device *pdev);

static struct platform_driver zx279133_lsp_clk_driver = {
	.probe = zx279133_clk_probe,
	.driver = {
		.name = "zx279133-lsp-clk",
		.of_match_table = zx279133_lsp_clk_of_match,
	},
};

static int zx279133_clk_probe(struct platform_device *pdev)
{
	if (of_device_is_compatible(pdev->dev.of_node, "zte,zx279133-topcrm"))
		return zx279133_topcrm_clk_probe(pdev);

	if (of_device_is_compatible(pdev->dev.of_node, "zte,zx279133-lsp1-clk"))
		return zx279133_lsp1_clk_probe(pdev);

	return zx279133_lsp0_clk_probe(pdev);
}
module_platform_driver(zx279133_lsp_clk_driver);

MODULE_DESCRIPTION("ZTE ZX279133 clock driver");
MODULE_LICENSE("GPL");
