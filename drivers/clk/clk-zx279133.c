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
#define ZX279133_TOPCRM_SYS_MUX_CTRL	0x00
#define ZX279133_TOPCRM_CPU_CCI_MUX_CTRL	0x04
#define ZX279133_TOPCRM_PON_NPPT_WCLK_MUX_CTRL	0x0c
#define ZX279133_TOPCRM_UNI_SERDES_GATE_CTRL	0x44
#define ZX279133_TOPCRM_PON_GATE_CTRL	0x48
#define ZX279133_TOPCRM_BUS_DIV_CTRL	0x5c
#define ZX279133_TOPCRM_NUM_CLKS	(ZX279133_TOPCRM_CLK_UNI_SERDES_50M + 1)
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
#define ZX279133_EFUSE_CLK_CTRL	0x54
#define ZX279133_EFUSE_PCLK_GATE	0
#define ZX279133_EFUSE_WCLK_GATE	1
#define ZX279133_PWM_CLK_CTRL	0x50
#define ZX279133_PWM_PCLK_GATE	0
#define ZX279133_PWM_WCLK_GATE	1
#define ZX279133_PWM_CLK_FLAGS	CLK_IGNORE_UNUSED
#define ZX279133_MDIO0_CLK_CTRL	0x04
#define ZX279133_MDIO1_CLK_CTRL	0x08
#define ZX279133_MDIO_PCLK_GATE	0
#define ZX279133_MDIO_WCLK_GATE	1
#define ZX279133_MDIO_RESET	4
#define ZX279133_MDIO_WCLK_DIV_SHIFT	11
#define ZX279133_MDIO_WCLK_DIV_WIDTH	7
#define ZX279133_MDIO_CLK_FLAGS	CLK_IGNORE_UNUSED
#define ZX279133_MDIO_RESET_DELAY_MIN_US	300
#define ZX279133_MDIO_RESET_DELAY_MAX_US	400
#define ZX279133_LSP1_NUM_CLKS	(ZX279133_LSP1_CLK_MDIO1_WCLK + 1)
#define ZX279133_TOPCRM_PVT_PCLK_GATE_CTRL	0x30
#define ZX279133_TOPCRM_PVT_PCLK_GATE	12
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

enum zx279133_topcrm_parent {
	ZX279133_PARENT_CLK25M,
	ZX279133_PARENT_CLK50M,
	ZX279133_PARENT_CLK100M,
	ZX279133_PARENT_CLK125M,
	ZX279133_PARENT_CLK200M,
	ZX279133_PARENT_CLK250M,
	ZX279133_PARENT_CLK344M,
	ZX279133_PARENT_CLK400M,
	ZX279133_PARENT_CLK416M,
	ZX279133_PARENT_CLK500M,
	ZX279133_PARENT_CLK688M,
	ZX279133_PARENT_CLK1000M_CPU,
	ZX279133_PARENT_CLK32K,
	ZX279133_PARENT_CLK172M,
	ZX279133_PARENT_CLK_PON_125M,
	ZX279133_PARENT_SYS_ACLK,
	ZX279133_PARENT_SYS_HCLK,
	ZX279133_PARENT_SYS_PCLK,
	ZX279133_PARENT_CCI_ACLK,
	ZX279133_PARENT_A53_MCLK,
	ZX279133_PARENT_PON_NPPT_WCLK_MUX,
	ZX279133_PARENT_COUNT,
};

struct zx279133_topcrm_gate_desc {
	const char *name;
	unsigned long flags;
	u16 reg_offset;
	u8 bit_idx;
	u8 parent;
};

struct zx279133_topcrm_clk {
	spinlock_t lock; /* Protects the shared TOPCRM gate register. */
	struct clk_hw *parents[ZX279133_PARENT_COUNT];
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
	spinlock_t lock; /* Protects the LSP1 clock control registers. */
	struct clk_divider wdt_dividers[2];
	struct clk_gate wdt_gates[2];
	struct clk_divider mdio_dividers[2];
	struct clk_gate mdio_gates[2];
	struct reset_controller_dev rcdev;
	void __iomem *mdio_regs[2];
	struct clk_hw_onecell_data data;
};

struct zx279133_topcrm_factor_desc {
	const char *name;
	const char *fw_name;
	u32 mult;
	u32 div;
};

static const struct zx279133_topcrm_factor_desc topcrm_factors[] = {
	[ZX279133_PARENT_CLK25M] = {
		.name = "clk25m", .fw_name = "osc25m", .mult = 1, .div = 1,
	},
	[ZX279133_PARENT_CLK50M] = {
		.name = "clk50m", .fw_name = "pll-lsp", .mult = 1, .div = 40,
	},
	[ZX279133_PARENT_CLK100M] = {
		.name = "clk100m", .fw_name = "pll-lsp", .mult = 1, .div = 20,
	},
	[ZX279133_PARENT_CLK125M] = {
		.name = "clk125m", .fw_name = "pll-lsp", .mult = 1, .div = 16,
	},
	[ZX279133_PARENT_CLK200M] = {
		.name = "clk200m", .fw_name = "pll-lsp", .mult = 1, .div = 10,
	},
	[ZX279133_PARENT_CLK250M] = {
		.name = "clk250m", .fw_name = "pll-lsp", .mult = 1, .div = 8,
	},
	[ZX279133_PARENT_CLK344M] = {
		.name = "clk344m", .fw_name = "pll-1376m", .mult = 1, .div = 4,
	},
	[ZX279133_PARENT_CLK400M] = {
		.name = "clk400m", .fw_name = "pll-lsp", .mult = 1, .div = 5,
	},
	[ZX279133_PARENT_CLK416M] = {
		.name = "clk416m", .fw_name = "pll-fpp", .mult = 1, .div = 6,
	},
	[ZX279133_PARENT_CLK500M] = {
		.name = "clk500m", .fw_name = "pll-lsp", .mult = 1, .div = 4,
	},
	[ZX279133_PARENT_CLK688M] = {
		.name = "clk688m", .fw_name = "pll-1376m", .mult = 1, .div = 2,
	},
	[ZX279133_PARENT_CLK1000M_CPU] = {
		.name = "clk1000m_cpu", .fw_name = "pll-cpu", .mult = 1, .div = 2,
	},
	[ZX279133_PARENT_CLK32K] = {
		.name = "clk32k768", .fw_name = "pll-1376m", .mult = 1,
		.div = 42000,
	},
	[ZX279133_PARENT_CLK172M] = {
		.name = "clk172m", .fw_name = "pll-1376m", .mult = 1, .div = 8,
	},
	[ZX279133_PARENT_CLK_PON_125M] = {
		.name = "clk_pon_125m", .fw_name = "pll-fpp", .mult = 1, .div = 20,
	},
};

/* ZX279133 stores the even divisor minus one. */
static const struct clk_div_table topcrm_even_div_table[] = {
	{ .val = 1, .div = 2 },
	{ .val = 3, .div = 4 },
	{ }
};

static const struct zx279133_topcrm_gate_desc topcrm_gates[] = {
	[ZX279133_TOPCRM_CLK_LSP0_100M] = {
		.name = "lsp0_100m",
		.flags = ZX279133_TOPCRM_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 12,
		.parent = ZX279133_PARENT_CLK100M,
	},
	[ZX279133_TOPCRM_CLK_LSP0_25M] = {
		.name = "lsp0_25m",
		.flags = ZX279133_TOPCRM_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 13,
		.parent = ZX279133_PARENT_CLK25M,
	},
	[ZX279133_TOPCRM_CLK_LSP0_PCLK] = {
		.name = "lsp0_pclk",
		.flags = ZX279133_TOPCRM_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 15,
		.parent = ZX279133_PARENT_SYS_PCLK,
	},
	[ZX279133_TOPCRM_CLK_LSP1_32K] = {
		.name = "lsp1_32k",
		.flags = 0,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 22,
		.parent = ZX279133_PARENT_CLK32K,
	},
	[ZX279133_TOPCRM_CLK_LSP1_PCLK] = {
		.name = "lsp1_pclk",
		.flags = 0,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 23,
		.parent = ZX279133_PARENT_SYS_PCLK,
	},
	[ZX279133_TOPCRM_CLK_LSP1_25M] = {
		.name = "lsp1_25m",
		.flags = 0,
		.reg_offset = ZX279133_TOPCRM_GATE_CTRL,
		.bit_idx = 21,
		.parent = ZX279133_PARENT_CLK25M,
	},
	[ZX279133_TOPCRM_CLK_TEMPSENSOR_WCLK] = {
		.name = "tempsensor_wclk",
		.flags = 0,
		.reg_offset = ZX279133_TOPCRM_PVT_GATE_CTRL,
		.bit_idx = 0,
	},
	[ZX279133_TOPCRM_CLK_SPIFC_WCLK] = {
		.name = "spifc_wclk",
		.flags = 0,
		.reg_offset = ZX279133_TOPCRM_SPIFC_GATE_CTRL,
		.bit_idx = ZX279133_TOPCRM_SPIFC_GATE_BIT,
		.parent = ZX279133_PARENT_CLK100M,
	},
	[ZX279133_TOPCRM_CLK_USB_PCLK] = {
		.name = "usb_pclk",
		.flags = ZX279133_TOPCRM_USB_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_USB_GATE_CTRL,
		.bit_idx = 4,
		.parent = ZX279133_PARENT_SYS_PCLK,
	},
	[ZX279133_TOPCRM_CLK_USB_CCI_ACLK] = {
		.name = "usb_cci_aclk",
		.flags = ZX279133_TOPCRM_USB_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_USB_GATE_CTRL,
		.bit_idx = 5,
		.parent = ZX279133_PARENT_CCI_ACLK,
	},
	[ZX279133_TOPCRM_CLK_USB_ACLK] = {
		.name = "usb_aclk",
		.flags = ZX279133_TOPCRM_USB_GATE_FLAGS,
		.reg_offset = ZX279133_TOPCRM_USB_GATE_CTRL,
		.bit_idx = 6,
		.parent = ZX279133_PARENT_SYS_ACLK,
	},
	[ZX279133_TOPCRM_CLK_PVT_PCLK] = {
		.name = "pvt_pclk",
		.flags = 0,
		.reg_offset = ZX279133_TOPCRM_PVT_PCLK_GATE_CTRL,
		.bit_idx = ZX279133_TOPCRM_PVT_PCLK_GATE,
		.parent = ZX279133_PARENT_CLK172M,
	},
	[ZX279133_TOPCRM_CLK_PON_IDM_ACLK] = {
		.name = "pon_idm_aclk",
		.reg_offset = ZX279133_TOPCRM_PON_GATE_CTRL,
		.bit_idx = 0,
		.parent = ZX279133_PARENT_SYS_ACLK,
	},
	[ZX279133_TOPCRM_CLK_PON_TM_ACLK] = {
		.name = "pon_tm_aclk",
		.reg_offset = ZX279133_TOPCRM_PON_GATE_CTRL,
		.bit_idx = 1,
		.parent = ZX279133_PARENT_SYS_ACLK,
	},
	[ZX279133_TOPCRM_CLK_PON_PCLK] = {
		.name = "pon_pclk",
		.reg_offset = ZX279133_TOPCRM_PON_GATE_CTRL,
		.bit_idx = 2,
		.parent = ZX279133_PARENT_SYS_PCLK,
	},
	[ZX279133_TOPCRM_CLK_PON_SMAC_WCLK] = {
		.name = "pon_smac_wclk",
		.reg_offset = ZX279133_TOPCRM_PON_GATE_CTRL,
		.bit_idx = 7,
		.parent = ZX279133_PARENT_CLK125M,
	},
	[ZX279133_TOPCRM_CLK_PON_MAC_WCLK] = {
		.name = "pon_mac_wclk",
		.reg_offset = ZX279133_TOPCRM_PON_GATE_CTRL,
		.bit_idx = 12,
		.parent = ZX279133_PARENT_CLK250M,
	},
};

static struct clk_hw *
zx279133_register_factor(struct device *dev,
			 const struct zx279133_topcrm_factor_desc *desc)
{
	return devm_clk_hw_register_fixed_factor_fwname(dev, dev->of_node,
			desc->name, desc->fw_name, 0, desc->mult, desc->div);
}

static struct clk_hw *
zx279133_register_mux(struct device *dev, const char *name,
		      const struct clk_parent_data *parents,
		      unsigned int num_parents, void __iomem *reg,
		      u8 shift, u8 width, u8 mux_flags, spinlock_t *lock)
{
	return devm_clk_hw_register_mux_parent_data_table(dev, name, parents,
			num_parents, CLK_GET_RATE_NOCACHE, reg, shift, width,
			mux_flags, NULL, lock);
}

static struct clk_hw *
zx279133_register_even_div(struct device *dev, const char *name,
			   const char *parent_name, void __iomem *reg,
			   u8 shift, spinlock_t *lock)
{
	return devm_clk_hw_register_divider_table(dev, name, parent_name,
			CLK_GET_RATE_NOCACHE, reg, shift, 2,
			CLK_DIVIDER_READ_ONLY, topcrm_even_div_table, lock);
}

static int zx279133_topcrm_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_topcrm_clk *priv;
	struct clk_parent_data sys_aclk_parents[4] = {};
	struct clk_parent_data a53_mclk_parents[8] = {};
	struct clk_parent_data cci_aclk_parents[8] = {};
	struct clk_parent_data pon_nppt_wclk_parents[8] = {};
	struct clk_hw *pvt_div;
	struct clk_hw *hw;
	void __iomem *base;
	void __iomem *reg;
	unsigned int index;
	int ret;

	priv = devm_kzalloc(dev,
			    struct_size(priv, data.hws, ZX279133_TOPCRM_NUM_CLKS),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	spin_lock_init(&priv->lock);
	for (index = 0; index < ARRAY_SIZE(topcrm_factors); index++) {
		const struct zx279133_topcrm_factor_desc *desc =
			&topcrm_factors[index];

		hw = zx279133_register_factor(dev, desc);
		if (IS_ERR(hw))
			return dev_err_probe(dev, PTR_ERR(hw),
					     "failed to register %s factor\n",
					     desc->name);

		priv->parents[index] = hw;
	}

	sys_aclk_parents[0].hw = priv->parents[ZX279133_PARENT_CLK25M];
	sys_aclk_parents[1].hw = priv->parents[ZX279133_PARENT_CLK100M];
	sys_aclk_parents[2].hw = priv->parents[ZX279133_PARENT_CLK200M];
	sys_aclk_parents[3].hw = priv->parents[ZX279133_PARENT_CLK250M];
	hw = zx279133_register_mux(dev, "sys_aclk", sys_aclk_parents,
				   ARRAY_SIZE(sys_aclk_parents),
				   base + ZX279133_TOPCRM_SYS_MUX_CTRL,
				   0, 2, CLK_MUX_READ_ONLY, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register sys_aclk mux\n");
	priv->parents[ZX279133_PARENT_SYS_ACLK] = hw;
	priv->data.hws[ZX279133_TOPCRM_CLK_SYS_ACLK] = hw;

	hw = zx279133_register_even_div(dev, "sys_hclk", "sys_aclk",
					base + ZX279133_TOPCRM_BUS_DIV_CTRL,
					0, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register sys_hclk divider\n");
	priv->parents[ZX279133_PARENT_SYS_HCLK] = hw;
	priv->data.hws[ZX279133_TOPCRM_CLK_SYS_HCLK] = hw;

	hw = zx279133_register_even_div(dev, "sys_pclk", "sys_aclk",
					base + ZX279133_TOPCRM_BUS_DIV_CTRL,
					4, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register sys_pclk divider\n");
	priv->parents[ZX279133_PARENT_SYS_PCLK] = hw;
	priv->data.hws[ZX279133_TOPCRM_CLK_SYS_PCLK] = hw;

	cci_aclk_parents[0].hw = priv->parents[ZX279133_PARENT_CLK25M];
	cci_aclk_parents[1].hw = priv->parents[ZX279133_PARENT_CLK100M];
	cci_aclk_parents[2].hw = priv->parents[ZX279133_PARENT_CLK200M];
	cci_aclk_parents[3].hw = priv->parents[ZX279133_PARENT_CLK250M];
	cci_aclk_parents[4].hw = priv->parents[ZX279133_PARENT_CLK344M];
	cci_aclk_parents[5].hw = priv->parents[ZX279133_PARENT_CLK400M];
	cci_aclk_parents[6].hw = priv->parents[ZX279133_PARENT_CLK688M];
	cci_aclk_parents[7].hw = priv->parents[ZX279133_PARENT_CLK500M];
	hw = zx279133_register_mux(dev, "cci_aclk", cci_aclk_parents,
				   ARRAY_SIZE(cci_aclk_parents),
				   base + ZX279133_TOPCRM_CPU_CCI_MUX_CTRL,
				   4, 3, CLK_MUX_READ_ONLY, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register cci_aclk mux\n");
	priv->parents[ZX279133_PARENT_CCI_ACLK] = hw;
	priv->data.hws[ZX279133_TOPCRM_CLK_CCI_ACLK] = hw;

	a53_mclk_parents[0].hw = priv->parents[ZX279133_PARENT_CLK25M];
	a53_mclk_parents[1].hw = priv->parents[ZX279133_PARENT_CLK100M];
	a53_mclk_parents[2].hw = priv->parents[ZX279133_PARENT_CLK250M];
	a53_mclk_parents[3].hw = priv->parents[ZX279133_PARENT_CLK400M];
	a53_mclk_parents[4].hw = priv->parents[ZX279133_PARENT_CLK500M];
	a53_mclk_parents[5].hw = priv->parents[ZX279133_PARENT_CLK688M];
	a53_mclk_parents[6].hw = priv->parents[ZX279133_PARENT_CLK416M];
	a53_mclk_parents[7].hw = priv->parents[ZX279133_PARENT_CLK1000M_CPU];
	hw = zx279133_register_mux(dev, "a53_mclk", a53_mclk_parents,
				   ARRAY_SIZE(a53_mclk_parents),
				   base + ZX279133_TOPCRM_CPU_CCI_MUX_CTRL,
				   0, 3,
				   IS_ENABLED(CONFIG_ZTE_ZX279133_CPUFREQ_PROBE) ?
					0 : CLK_MUX_READ_ONLY,
				   &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register a53_mclk mux\n");
	priv->parents[ZX279133_PARENT_A53_MCLK] = hw;
	priv->data.hws[ZX279133_TOPCRM_CLK_A53_MCLK] = hw;

	hw = priv->parents[ZX279133_PARENT_CLK50M];
	pvt_div = devm_clk_hw_register_divider_parent_hw(dev, "clk1m22", hw, 0,
							 base + ZX279133_TOPCRM_PVT_DIV_CTRL,
							 0, 6, CLK_DIVIDER_READ_ONLY,
							 &priv->lock);
	if (IS_ERR(pvt_div))
		return dev_err_probe(dev, PTR_ERR(pvt_div),
				     "failed to register clk1m22 divider\n");

	pon_nppt_wclk_parents[0].hw = priv->parents[ZX279133_PARENT_CLK25M];
	pon_nppt_wclk_parents[1].hw = priv->parents[ZX279133_PARENT_CLK100M];
	pon_nppt_wclk_parents[2].hw = priv->parents[ZX279133_PARENT_CLK200M];
	pon_nppt_wclk_parents[3].hw = priv->parents[ZX279133_PARENT_CLK344M];
	pon_nppt_wclk_parents[4].hw = priv->parents[ZX279133_PARENT_CLK400M];
	pon_nppt_wclk_parents[5].hw = priv->parents[ZX279133_PARENT_CLK500M];
	pon_nppt_wclk_parents[6].hw = priv->parents[ZX279133_PARENT_CLK416M];
	pon_nppt_wclk_parents[7].hw = priv->parents[ZX279133_PARENT_CLK_PON_125M];
	hw = zx279133_register_mux(dev, "pon_nppt_wclk_mux",
				   pon_nppt_wclk_parents,
				   ARRAY_SIZE(pon_nppt_wclk_parents),
				   base + ZX279133_TOPCRM_PON_NPPT_WCLK_MUX_CTRL,
				   24, 3, CLK_MUX_READ_ONLY, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register pon_nppt_wclk mux\n");
	priv->parents[ZX279133_PARENT_PON_NPPT_WCLK_MUX] = hw;
	priv->data.hws[ZX279133_TOPCRM_CLK_PON_NPPT_WCLK_MUX] = hw;

	hw = devm_clk_hw_register_gate_parent_hw(dev, "uni_serdes_pclk",
						 priv->parents[ZX279133_PARENT_SYS_PCLK],
						 0,
						 base + ZX279133_TOPCRM_UNI_SERDES_GATE_CTRL,
						 8, 0, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register uni_serdes_pclk gate\n");
	priv->data.hws[ZX279133_TOPCRM_CLK_UNI_SERDES_PCLK] = hw;

	hw = devm_clk_hw_register_gate_parent_hw(dev, "uni_serdes_50m",
						 priv->parents[ZX279133_PARENT_CLK50M],
						 0,
						 base + ZX279133_TOPCRM_UNI_SERDES_GATE_CTRL,
						 9, 0, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register uni_serdes_50m gate\n");
	priv->data.hws[ZX279133_TOPCRM_CLK_UNI_SERDES_50M] = hw;

	hw = devm_clk_hw_register_gate_parent_hw(dev, "pon_serdes_pclk",
						 priv->parents[ZX279133_PARENT_SYS_PCLK],
						 0,
						 base + ZX279133_TOPCRM_UNI_SERDES_GATE_CTRL,
						 0, 0, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register pon_serdes_pclk gate\n");
	priv->data.hws[ZX279133_TOPCRM_CLK_PON_SERDES_PCLK] = hw;

	/* NPPT uses the vendor-named PON WOE1 working-clock path. */
	hw = devm_clk_hw_register_gate_parent_hw(dev, "pon_woe1_wclk",
						 priv->parents[ZX279133_PARENT_PON_NPPT_WCLK_MUX],
						 0,
						 base + ZX279133_TOPCRM_PON_GATE_CTRL,
						 10, 0, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register pon_woe1_wclk gate\n");
	priv->data.hws[ZX279133_TOPCRM_CLK_PON_WOE1_WCLK] = hw;

	for (index = 0; index < ARRAY_SIZE(topcrm_gates); index++) {
		const struct zx279133_topcrm_gate_desc *desc =
			&topcrm_gates[index];

		if (!desc->name)
			continue;

		reg = base + desc->reg_offset;
		if (index == ZX279133_TOPCRM_CLK_TEMPSENSOR_WCLK) {
			hw = devm_clk_hw_register_gate_parent_hw(dev, desc->name,
								 pvt_div, desc->flags,
								 reg, desc->bit_idx, 0,
								 &priv->lock);
		} else {
			struct clk_hw *parent = priv->parents[desc->parent];

			hw = devm_clk_hw_register_gate_parent_hw(dev, desc->name,
								 parent, desc->flags,
								 reg, desc->bit_idx, 0,
								 &priv->lock);
		}
		if (IS_ERR(hw))
			return dev_err_probe(dev, PTR_ERR(hw),
					     "failed to register %s gate\n",
					     desc->name);

		priv->data.hws[index] = hw;
	}

	hw = devm_clk_hw_register_gate_parent_hw(dev, "lsp1_100m",
						 priv->parents[ZX279133_PARENT_CLK100M],
						 ZX279133_TOPCRM_GATE_FLAGS,
						 base + ZX279133_TOPCRM_GATE_CTRL,
						 20, 0, &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register lsp1_100m gate\n");
	priv->data.hws[ZX279133_TOPCRM_CLK_LSP1_100M] = hw;

	priv->data.num = ZX279133_TOPCRM_NUM_CLKS;
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

static const struct clk_parent_data lsp1_pclk_parent = {
	.fw_name = "pclk",
};

static const struct clk_parent_data lsp1_wclk32k_parent = {
	.fw_name = "wclk32k",
};

static const struct clk_parent_data lsp1_wclk25m_parent = {
	.fw_name = "wclk25m",
};

static const struct clk_parent_data lsp1_wclk100m_parent = {
	.fw_name = "wclk100m",
};

static int zx279133_lsp1_reset_update(struct reset_controller_dev *rcdev,
				      unsigned long id, bool deassert)
{
	struct zx279133_lsp1_clk *priv =
		container_of(rcdev, struct zx279133_lsp1_clk, rcdev);
	unsigned long flags;
	u32 value;

	if (id >= ARRAY_SIZE(priv->mdio_regs))
		return -EINVAL;

	spin_lock_irqsave(&priv->lock, flags);
	value = readl(priv->mdio_regs[id]);
	if (deassert)
		value |= BIT(ZX279133_MDIO_RESET);
	else
		value &= ~BIT(ZX279133_MDIO_RESET);
	writel(value, priv->mdio_regs[id]);
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int zx279133_lsp1_reset_assert(struct reset_controller_dev *rcdev,
				      unsigned long id)
{
	return zx279133_lsp1_reset_update(rcdev, id, false);
}

static int zx279133_lsp1_reset_deassert(struct reset_controller_dev *rcdev,
					unsigned long id)
{
	return zx279133_lsp1_reset_update(rcdev, id, true);
}

static int zx279133_lsp1_reset_reset(struct reset_controller_dev *rcdev,
				     unsigned long id)
{
	int ret;

	ret = zx279133_lsp1_reset_assert(rcdev, id);
	if (ret)
		return ret;

	/* Match the vendor driver's assertion and post-deassertion delays. */
	usleep_range(ZX279133_MDIO_RESET_DELAY_MIN_US,
		     ZX279133_MDIO_RESET_DELAY_MAX_US);

	ret = zx279133_lsp1_reset_deassert(rcdev, id);
	if (ret)
		return ret;

	usleep_range(ZX279133_MDIO_RESET_DELAY_MIN_US,
		     ZX279133_MDIO_RESET_DELAY_MAX_US);

	return 0;
}

static int zx279133_lsp1_reset_status(struct reset_controller_dev *rcdev,
				      unsigned long id)
{
	struct zx279133_lsp1_clk *priv =
		container_of(rcdev, struct zx279133_lsp1_clk, rcdev);

	if (id >= ARRAY_SIZE(priv->mdio_regs))
		return -EINVAL;

	return !(readl(priv->mdio_regs[id]) & BIT(ZX279133_MDIO_RESET));
}

static const struct reset_control_ops zx279133_lsp1_reset_ops = {
	.assert = zx279133_lsp1_reset_assert,
	.deassert = zx279133_lsp1_reset_deassert,
	.reset = zx279133_lsp1_reset_reset,
	.status = zx279133_lsp1_reset_status,
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
	static const unsigned int mdio_offsets[] = {
		ZX279133_MDIO0_CLK_CTRL, ZX279133_MDIO1_CLK_CTRL,
	};
	static const char * const mdio_names[][2] = {
		{ "mdio0_pclk", "mdio0_wclk" },
		{ "mdio1_pclk", "mdio1_wclk" },
	};
	struct device *dev = &pdev->dev;
	struct zx279133_lsp1_clk *priv;
	struct clk_hw_onecell_data *data;
	struct clk_hw *hw;
	void __iomem *base;
	void __iomem *reg;
	unsigned int index;
	int ret;

	priv = devm_kzalloc(dev, struct_size(priv, data.hws, ZX279133_LSP1_NUM_CLKS), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	spin_lock_init(&priv->lock);
	for (index = 0; index < ARRAY_SIZE(offsets); index++) {
		struct clk_divider *divider = &priv->wdt_dividers[index];
		struct clk_gate *gate = &priv->wdt_gates[index];

		reg = base + offsets[index];
		hw = devm_clk_hw_register_gate_parent_data(dev, names[index][0],
							   &lsp1_pclk_parent,
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
							  &lsp1_wclk32k_parent, 1,
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

	reg = base + ZX279133_EFUSE_CLK_CTRL;
	hw = devm_clk_hw_register_gate_parent_data(dev, "efuse_pclk",
						   &lsp1_pclk_parent, 0, reg,
						   ZX279133_EFUSE_PCLK_GATE, 0,
						   &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register efuse PCLK\n");
	priv->data.hws[ZX279133_LSP1_CLK_EFUSE_PCLK] = hw;

	hw = devm_clk_hw_register_gate_parent_data(dev, "efuse_wclk",
						   &lsp1_wclk25m_parent, 0, reg,
						   ZX279133_EFUSE_WCLK_GATE, 0,
						   &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register efuse WCLK\n");
	priv->data.hws[ZX279133_LSP1_CLK_EFUSE_WCLK] = hw;

	/* Preserve the firmware PWM clock state until a safe consumer exists. */
	reg = base + ZX279133_PWM_CLK_CTRL;
	hw = devm_clk_hw_register_gate_parent_data(dev, "pwm_pclk",
						   &lsp1_pclk_parent,
						   ZX279133_PWM_CLK_FLAGS, reg,
						   ZX279133_PWM_PCLK_GATE, 0,
						   &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register PWM PCLK\n");
	priv->data.hws[ZX279133_LSP1_CLK_PWM_PCLK] = hw;

	hw = devm_clk_hw_register_gate_parent_data(dev, "pwm_wclk",
						   &lsp1_wclk25m_parent,
						   ZX279133_PWM_CLK_FLAGS, reg,
						   ZX279133_PWM_WCLK_GATE, 0,
						   &priv->lock);
	if (IS_ERR(hw))
		return dev_err_probe(dev, PTR_ERR(hw),
				     "failed to register PWM WCLK\n");
	priv->data.hws[ZX279133_LSP1_CLK_PWM_WCLK] = hw;

	for (index = 0; index < ARRAY_SIZE(mdio_offsets); index++) {
		struct clk_divider *divider = &priv->mdio_dividers[index];
		struct clk_gate *gate = &priv->mdio_gates[index];
		unsigned int clk_id = ZX279133_LSP1_CLK_MDIO0_PCLK + index * 2;

		reg = base + mdio_offsets[index];
		priv->mdio_regs[index] = reg;
		hw = devm_clk_hw_register_gate_parent_data(dev,
							   mdio_names[index][0],
							   &lsp1_pclk_parent,
							   ZX279133_MDIO_CLK_FLAGS,
							   reg,
							   ZX279133_MDIO_PCLK_GATE,
							   0, &priv->lock);
		if (IS_ERR(hw))
			return dev_err_probe(dev, PTR_ERR(hw),
					     "failed to register MDIO%u PCLK\n",
					     index);
		priv->data.hws[clk_id] = hw;

		divider->reg = reg;
		divider->shift = ZX279133_MDIO_WCLK_DIV_SHIFT;
		divider->width = ZX279133_MDIO_WCLK_DIV_WIDTH;
		divider->flags = CLK_DIVIDER_READ_ONLY;
		divider->lock = &priv->lock;

		gate->reg = reg;
		gate->bit_idx = ZX279133_MDIO_WCLK_GATE;
		gate->lock = &priv->lock;

		hw = devm_clk_hw_register_composite_pdata(dev,
							  mdio_names[index][1],
							  &lsp1_wclk100m_parent, 1,
							  NULL, NULL,
							  &divider->hw,
							  &clk_divider_ro_ops,
							  &gate->hw,
							  &clk_gate_ops,
							  ZX279133_MDIO_CLK_FLAGS);
		if (IS_ERR(hw))
			return dev_err_probe(dev, PTR_ERR(hw),
					     "failed to register MDIO%u WCLK\n",
					     index);
		priv->data.hws[clk_id + 1] = hw;
	}

	priv->data.num = ZX279133_LSP1_NUM_CLKS;
	data = &priv->data;
	priv->rcdev.ops = &zx279133_lsp1_reset_ops;
	priv->rcdev.owner = THIS_MODULE;
	priv->rcdev.of_node = dev->of_node;
	priv->rcdev.nr_resets = ARRAY_SIZE(priv->mdio_regs);

	ret = devm_reset_controller_register(dev, &priv->rcdev);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to register LSP1 reset controller\n");

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, data);
	if (ret)
		return ret;

	dev_info(dev, "registered %u LSP1 clocks\n", priv->data.num);

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
