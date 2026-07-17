// SPDX-License-Identifier: GPL-2.0-only
/*
 * Read-only audit support for the firmware-configured ZX279133 PWM block.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define ZX279133_PWM_CHANNELS	4
#define ZX279133_PWM_CHANNEL_STRIDE	0x10
#define ZX279133_PWM_CHANNEL_BASE	0x10
#define ZX279133_PWM_MODE		0x0
#define ZX279133_PWM_PERIOD		0x4
#define ZX279133_PWM_DUTY		0x8
#define ZX279133_PWM_PCLK_RATE		125000000
#define ZX279133_PWM_WCLK_RATE		25000000

static int zx279133_pwm_audit_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct clk *pclk, *wclk;
	void __iomem *base;
	unsigned int channel;
	unsigned long pclk_rate, wclk_rate;

	pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(pclk))
		return dev_err_probe(dev, PTR_ERR(pclk), "failed to get PCLK\n");

	wclk = devm_clk_get(dev, "wclk");
	if (IS_ERR(wclk))
		return dev_err_probe(dev, PTR_ERR(wclk), "failed to get WCLK\n");

	pclk_rate = clk_get_rate(pclk);
	wclk_rate = clk_get_rate(wclk);
	if (pclk_rate != ZX279133_PWM_PCLK_RATE ||
	    wclk_rate != ZX279133_PWM_WCLK_RATE)
		return dev_err_probe(dev, -EINVAL,
				     "unexpected clock rates pclk=%lu wclk=%lu\n",
				     pclk_rate, wclk_rate);

	/* Never enable a gate merely to inspect firmware-owned controller state. */
	if (!__clk_is_enabled(pclk) || !__clk_is_enabled(wclk))
		return dev_err_probe(dev, -EBUSY,
				     "firmware clock gate is disabled\n");

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	dev_info(dev, "firmware clocks pclk=%lu:Y wclk=%lu:Y\n",
		 pclk_rate, wclk_rate);

	for (channel = 0; channel < ZX279133_PWM_CHANNELS; channel++) {
		void __iomem *regs = base + ZX279133_PWM_CHANNEL_BASE +
			channel * ZX279133_PWM_CHANNEL_STRIDE;

		dev_info(dev, "channel%u mode=%#x period=%#x duty=%#x\n", channel,
			 readl(regs + ZX279133_PWM_MODE),
			 readl(regs + ZX279133_PWM_PERIOD),
			 readl(regs + ZX279133_PWM_DUTY));
	}

	return 0;
}

static const struct of_device_id zx279133_pwm_audit_of_match[] = {
	{ .compatible = "zte,zx279133-pwm-audit" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_pwm_audit_of_match);

static struct platform_driver zx279133_pwm_audit_driver = {
	.probe = zx279133_pwm_audit_probe,
	.driver = {
		.name = "zx279133-pwm-audit",
		.of_match_table = zx279133_pwm_audit_of_match,
	},
};
module_platform_driver(zx279133_pwm_audit_driver);

MODULE_DESCRIPTION("ZTE ZX279133 PWM read-only audit driver");
MODULE_LICENSE("GPL");
