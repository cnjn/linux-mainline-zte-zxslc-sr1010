// SPDX-License-Identifier: GPL-2.0-only
/*
 * Read-only audit support for the firmware-configured ZX279133 PWM block.
 */

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

static int zx279133_pwm_audit_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	void __iomem *base;
	unsigned int channel;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

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
