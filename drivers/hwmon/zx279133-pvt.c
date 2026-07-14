// SPDX-License-Identifier: GPL-2.0-only
/*
 * ZTE ZX279133 CLN22ULP process/voltage/temperature sensor.
 *
 * The hardware exposes one temperature conversion channel.  The vendor
 * implementation starts a conversion for each read. This driver keeps the
 * same one-shot behavior and exposes no writable hwmon files.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/hwmon.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define ZX279133_PVT_CTRL		0x00
#define ZX279133_PVT_TRIM		0x04
#define ZX279133_PVT_STATUS		0x08
#define ZX279133_PVT_DATA		0x10
#define ZX279133_PVT_CONFIG		0x20

#define ZX279133_PVT_READY		BIT(10)
#define ZX279133_PVT_DATA_MASK		GENMASK(9, 0)
#define ZX279133_PVT_CONFIG_MASK		GENMASK(7, 4)
#define ZX279133_PVT_TRIM_MASK		GENMASK(4, 0)
#define ZX279133_PVT_TEMP_CTRL		0x13

#define ZX279133_PVT_POLL_US		5
#define ZX279133_PVT_TIMEOUT_US		500000
#define ZX279133_PVT_DEFAULT_TRIM	15

/* The vendor DTS coefficients, with the vendor's 1e6 coefficient multiple. */
#define ZX279133_PVT_MC_DIV		1000000000000LL

struct zx279133_pvt_coeff {
	s64 a4;
	s64 a3;
	s64 a2;
	s64 a1;
	s64 a0;
};

struct zx279133_pvt {
	void __iomem *base;
	struct clk *wclk;
	struct zx279133_pvt_coeff coeff;
	struct mutex lock; /* Serializes one-shot conversions. */
	u8 trim;
	bool trim_from_nvmem;
};

static const struct zx279133_pvt_coeff zx279133_pvt_cln22ulp_coeff = {
	.a4 = -25761LL,
	.a3 = 97332000LL,
	.a2 = -191650000000LL,
	.a1 = 307620000000000LL,
	.a0 = -52156000000000000LL,
};

static long zx279133_pvt_calc_temp(const struct zx279133_pvt *pvt, u32 raw)
{
	s64 value;

	/* Horner form matches the vendor a4..a0 polynomial and avoids overflow. */
	value = pvt->coeff.a4;
	value = value * raw + pvt->coeff.a3;
	value = value * raw + pvt->coeff.a2;
	value = value * raw + pvt->coeff.a1;
	value = value * raw + pvt->coeff.a0;

	/* hwmon reports milli-Celsius; the polynomial is in degrees Celsius. */
	return div64_s64(value, ZX279133_PVT_MC_DIV);
}

static int zx279133_pvt_sample(struct zx279133_pvt *pvt, u32 *raw)
{
	u32 status;
	int ret;

	mutex_lock(&pvt->lock);

	/* This is the one-shot sequence used by the vendor driver. */
	writel(0, pvt->base + ZX279133_PVT_CTRL);
	fsleep(10);
	writel(pvt->trim & ZX279133_PVT_TRIM_MASK,
	       pvt->base + ZX279133_PVT_TRIM);
	writel(ZX279133_PVT_TEMP_CTRL, pvt->base + ZX279133_PVT_CTRL);
	fsleep(10);

	ret = readl_poll_timeout(pvt->base + ZX279133_PVT_STATUS, status,
				 status & ZX279133_PVT_READY,
				 ZX279133_PVT_POLL_US, ZX279133_PVT_TIMEOUT_US);
	if (ret)
		goto out_unlock;

	*raw = readl(pvt->base + ZX279133_PVT_DATA) & ZX279133_PVT_DATA_MASK;

out_unlock:
	mutex_unlock(&pvt->lock);
	return ret;
}

static int zx279133_pvt_read(struct device *dev,
			     enum hwmon_sensor_types type, u32 attr,
			     int channel, long *val)
{
	struct zx279133_pvt *pvt = dev_get_drvdata(dev);
	u32 raw;
	int ret;

	if (type != hwmon_temp || attr != hwmon_temp_input || channel != 0)
		return -EOPNOTSUPP;

	ret = zx279133_pvt_sample(pvt, &raw);
	if (ret)
		return ret;

	*val = zx279133_pvt_calc_temp(pvt, raw);
	return 0;
}

static umode_t zx279133_pvt_is_visible(const void *data,
				       enum hwmon_sensor_types type,
				       u32 attr, int channel)
{
	if (type == hwmon_temp && attr == hwmon_temp_input && channel == 0)
		return 0444;

	return 0;
}

static const struct hwmon_channel_info * const zx279133_pvt_info[] = {
	HWMON_CHANNEL_INFO(chip, HWMON_C_REGISTER_TZ),
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT),
	NULL,
};

static const struct hwmon_ops zx279133_pvt_hwmon_ops = {
	.is_visible = zx279133_pvt_is_visible,
	.read = zx279133_pvt_read,
};

static const struct hwmon_chip_info zx279133_pvt_chip_info = {
	.ops = &zx279133_pvt_hwmon_ops,
	.info = zx279133_pvt_info,
};

static int zx279133_pvt_get_trim(struct device *dev,
				 struct zx279133_pvt *pvt)
{
	u8 trim;
	int ret;

	ret = nvmem_cell_read_u8(dev, "ttrim", &trim);
	if (!ret) {
		if (trim > 0x1f)
			return dev_err_probe(dev, -ERANGE,
					    "ttrim value %u exceeds 5-bit field\n",
					    trim);
		pvt->trim = trim;
		pvt->trim_from_nvmem = true;
		return 0;
	}

	/* The vendor driver falls back to 15 when no efuse value is available. */
	if (ret != -ENOENT && ret != -EOPNOTSUPP)
		return dev_err_probe(dev, ret, "failed to read ttrim NVMEM cell\n");

	pvt->trim = ZX279133_PVT_DEFAULT_TRIM;
	dev_warn(dev,
		 "ttrim NVMEM cell unavailable; using vendor fallback %u (uncalibrated)\n",
		 pvt->trim);
	return 0;
}

static int zx279133_pvt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_pvt *pvt;
	struct device *hwmon;
	u32 config, raw;
	unsigned long rate;
	int ret;

	pvt = devm_kzalloc(dev, sizeof(*pvt), GFP_KERNEL);
	if (!pvt)
		return -ENOMEM;

	pvt->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pvt->base))
		return PTR_ERR(pvt->base);
	mutex_init(&pvt->lock);

	pvt->wclk = devm_clk_get_enabled(dev, "wclk");
	if (IS_ERR(pvt->wclk))
		return dev_err_probe(dev, PTR_ERR(pvt->wclk),
				    "failed to enable working clock\n");

	rate = clk_get_rate(pvt->wclk);
	if (!rate)
		return dev_err_probe(dev, -EINVAL, "working clock has zero rate\n");

	pvt->coeff = zx279133_pvt_cln22ulp_coeff;
	ret = zx279133_pvt_get_trim(dev, pvt);
	if (ret)
		return ret;

	config = readl(pvt->base + ZX279133_PVT_CONFIG);
	writel(config | ZX279133_PVT_CONFIG_MASK,
	       pvt->base + ZX279133_PVT_CONFIG);

	/* Fail probe if the sensor cannot complete a first conversion. */
	ret = zx279133_pvt_sample(pvt, &raw);
	if (ret)
		return dev_err_probe(dev, ret, "initial conversion timed out\n");

	platform_set_drvdata(pdev, pvt);
	hwmon = devm_hwmon_device_register_with_info(dev, "zx279133_pvt",
						     pvt,
						     &zx279133_pvt_chip_info,
						     NULL);
	if (IS_ERR(hwmon))
		return dev_err_probe(dev, PTR_ERR(hwmon),
				    "failed to register hwmon device\n");

	dev_info(dev, "ready: raw=%u temp=%ld mC clock=%lu Hz%s\n", raw,
		 zx279133_pvt_calc_temp(pvt, raw), rate,
		 pvt->trim_from_nvmem ? "" : " (fallback trim)");
	return 0;
}

static const struct of_device_id zx279133_pvt_of_match[] = {
	{ .compatible = "zte,zx279133-pvt" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_pvt_of_match);

static struct platform_driver zx279133_pvt_driver = {
	.probe = zx279133_pvt_probe,
	.driver = {
		.name = "zx279133-pvt",
		.of_match_table = zx279133_pvt_of_match,
	},
};
module_platform_driver(zx279133_pvt_driver);

MODULE_DESCRIPTION("ZTE ZX279133 CLN22ULP PVT temperature sensor");
MODULE_LICENSE("GPL");
