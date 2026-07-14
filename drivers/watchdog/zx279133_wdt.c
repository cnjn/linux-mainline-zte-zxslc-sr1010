// SPDX-License-Identifier: GPL-2.0-only

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/watchdog.h>

#define ZX279133_WDT_CONFIG		0x04
#define ZX279133_WDT_LOAD		0x08
#define ZX279133_WDT_STATUS		0x10
#define ZX279133_WDT_FEED		0x18
#define ZX279133_WDT_LOCK		0x1c

#define ZX279133_WDT_KEY			0x12340000
#define ZX279133_WDT_CONFIG_VALUE	(ZX279133_WDT_KEY | 0x1f00)
#define ZX279133_WDT_FEED_MASK		0x3c
#define ZX279133_WDT_LOCKED		BIT(0)
#define ZX279133_WDT_READY		BIT(1)
#define ZX279133_WDT_DIVIDER		32
#define ZX279133_WDT_INPUT_RATE		32768
#define ZX279133_WDT_MAX_COUNT		0xffff
#define ZX279133_WDT_DEFAULT_TIMEOUT	30
#define ZX279133_WDT_RESTART_TIMEOUT	1
#define ZX279133_WDT_RESTART_WAIT_MS	1500

static unsigned int timeout;
module_param(timeout, uint, 0444);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (default=30)");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0444);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started");

struct zx279133_wdt {
	struct watchdog_device wdd;
	struct clk_bulk_data clocks[2];
	void __iomem *base;
	unsigned long count_rate;
};

static void zx279133_wdt_disable_clocks(void *data)
{
	struct zx279133_wdt *wdt = data;

	clk_bulk_disable_unprepare(ARRAY_SIZE(wdt->clocks), wdt->clocks);
}

static void zx279133_wdt_feed(struct zx279133_wdt *wdt)
{
	u32 value = readl(wdt->base + ZX279133_WDT_FEED);

	writel(ZX279133_WDT_KEY | (value ^ ZX279133_WDT_FEED_MASK),
	       wdt->base + ZX279133_WDT_FEED);
}

static int zx279133_wdt_stop(struct watchdog_device *wdd)
{
	struct zx279133_wdt *wdt = watchdog_get_drvdata(wdd);

	writel(ZX279133_WDT_KEY, wdt->base + ZX279133_WDT_LOCK);
	readl(wdt->base + ZX279133_WDT_LOCK);

	return 0;
}

static int zx279133_wdt_start(struct watchdog_device *wdd)
{
	struct zx279133_wdt *wdt = watchdog_get_drvdata(wdd);
	u64 count = (u64)wdt->count_rate * wdd->timeout - 1;
	u32 status;
	int ret;

	if (count > ZX279133_WDT_MAX_COUNT)
		return -EINVAL;

	writel(ZX279133_WDT_KEY, wdt->base + ZX279133_WDT_LOCK);
	writel(ZX279133_WDT_CONFIG_VALUE, wdt->base + ZX279133_WDT_CONFIG);
	writel(ZX279133_WDT_KEY | (u32)count,
	       wdt->base + ZX279133_WDT_LOAD);
	zx279133_wdt_feed(wdt);

	ret = readl_poll_timeout_atomic(wdt->base + ZX279133_WDT_STATUS,
					status, status & ZX279133_WDT_READY,
					1, 10000);
	if (ret) {
		zx279133_wdt_stop(wdd);
		return ret;
	}

	writel(ZX279133_WDT_KEY | ZX279133_WDT_LOCKED,
	       wdt->base + ZX279133_WDT_LOCK);

	return 0;
}

static int zx279133_wdt_ping(struct watchdog_device *wdd)
{
	struct zx279133_wdt *wdt = watchdog_get_drvdata(wdd);

	zx279133_wdt_feed(wdt);

	return 0;
}

static int zx279133_wdt_set_timeout(struct watchdog_device *wdd,
				    unsigned int timeout)
{
	unsigned int old_timeout = wdd->timeout;
	int ret;

	wdd->timeout = timeout;
	ret = zx279133_wdt_start(wdd);
	if (!ret) {
		/* The first core ping must not race the hardware reload sequence. */
		usleep_range(1000, 2000);
		return 0;
	}

	wdd->timeout = old_timeout;
	if (zx279133_wdt_start(wdd)) {
		clear_bit(WDOG_HW_RUNNING, &wdd->status);
		dev_crit(wdd->parent,
			 "failed to restore watchdog after timeout update\n");
	}

	return ret;
}

static int zx279133_wdt_restart(struct watchdog_device *wdd,
				unsigned long action, void *data)
{
	int ret;

	wdd->timeout = ZX279133_WDT_RESTART_TIMEOUT;
	zx279133_wdt_stop(wdd);

	ret = zx279133_wdt_start(wdd);
	if (ret)
		return ret;

	mdelay(ZX279133_WDT_RESTART_WAIT_MS);

	return 0;
}

static const struct watchdog_info zx279133_wdt_info = {
	.identity = "ZX279133 Watchdog",
	.options = WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE | WDIOF_SETTIMEOUT,
};

static const struct watchdog_ops zx279133_wdt_ops = {
	.owner = THIS_MODULE,
	.start = zx279133_wdt_start,
	.stop = zx279133_wdt_stop,
	.ping = zx279133_wdt_ping,
	.set_timeout = zx279133_wdt_set_timeout,
};

static const struct watchdog_ops zx279133_wdt_restart_ops = {
	.owner = THIS_MODULE,
	.start = zx279133_wdt_start,
	.stop = zx279133_wdt_stop,
	.ping = zx279133_wdt_ping,
	.set_timeout = zx279133_wdt_set_timeout,
	.restart = zx279133_wdt_restart,
};

static void zx279133_wdt_stop_action(void *data)
{
	struct zx279133_wdt *wdt = data;

	zx279133_wdt_stop(&wdt->wdd);
	clear_bit(WDOG_HW_RUNNING, &wdt->wdd.status);
}

static int zx279133_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_wdt *wdt;
	struct regmap *reset;
	unsigned long rate;
	u32 reset_offset;
	u32 reset_mask;
	u32 reset_value;
	int ret;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wdt->base))
		return PTR_ERR(wdt->base);

	wdt->clocks[0].id = "pclk";
	wdt->clocks[1].id = "wdtclk";
	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(wdt->clocks), wdt->clocks);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get clocks\n");

	ret = clk_bulk_prepare_enable(ARRAY_SIZE(wdt->clocks), wdt->clocks);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable clocks\n");

	ret = devm_add_action_or_reset(dev, zx279133_wdt_disable_clocks, wdt);
	if (ret)
		return ret;

	ret = clk_set_rate(wdt->clocks[1].clk, ZX279133_WDT_INPUT_RATE);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to set watchdog clock rate\n");

	rate = clk_get_rate(wdt->clocks[1].clk);
	if (rate != ZX279133_WDT_INPUT_RATE)
		return dev_err_probe(dev, -EINVAL,
				     "watchdog clock rate is %lu, expected %u\n",
				     rate, ZX279133_WDT_INPUT_RATE);

	wdt->count_rate = rate / ZX279133_WDT_DIVIDER;
	if (!wdt->count_rate || rate % ZX279133_WDT_DIVIDER)
		return dev_err_probe(dev, -EINVAL,
				     "invalid watchdog clock rate %lu\n", rate);

	reset = syscon_regmap_lookup_by_phandle(dev->of_node,
						"zte,reset-controller");
	if (IS_ERR(reset))
		return dev_err_probe(dev, PTR_ERR(reset),
				     "failed to get reset controller\n");

	ret = of_property_read_u32(dev->of_node, "zte,reset-offset",
				   &reset_offset);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get reset offset\n");

	ret = of_property_read_u32(dev->of_node, "zte,reset-mask", &reset_mask);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get reset mask\n");
	if (!reset_mask)
		return dev_err_probe(dev, -EINVAL, "reset mask must not be zero\n");

	ret = regmap_update_bits(reset, reset_offset, reset_mask, reset_mask);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable reset output\n");

	ret = regmap_read(reset, reset_offset, &reset_value);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read reset mask\n");
	if ((reset_value & reset_mask) != reset_mask)
		return dev_err_probe(dev, -EIO, "reset mask did not latch\n");

	wdt->wdd.info = &zx279133_wdt_info;
	if (of_property_read_bool(dev->of_node, "zte,restart-watchdog")) {
		wdt->wdd.ops = &zx279133_wdt_restart_ops;
		watchdog_set_restart_priority(&wdt->wdd, 192);
	} else {
		wdt->wdd.ops = &zx279133_wdt_ops;
	}
	wdt->wdd.parent = dev;
	wdt->wdd.min_timeout = 1;
	wdt->wdd.max_timeout = ZX279133_WDT_MAX_COUNT / wdt->count_rate;
	wdt->wdd.timeout = min_t(unsigned int, ZX279133_WDT_DEFAULT_TIMEOUT,
				 wdt->wdd.max_timeout);
	watchdog_set_drvdata(&wdt->wdd, wdt);
	watchdog_set_nowayout(&wdt->wdd, nowayout);
	watchdog_init_timeout(&wdt->wdd, timeout, dev);
	watchdog_stop_on_reboot(&wdt->wdd);

	ret = zx279133_wdt_start(&wdt->wdd);
	if (ret)
		return dev_err_probe(dev, ret, "failed to start watchdog\n");

	set_bit(WDOG_HW_RUNNING, &wdt->wdd.status);
	ret = devm_watchdog_register_device(dev, &wdt->wdd);
	if (ret) {
		zx279133_wdt_stop_action(wdt);
		return dev_err_probe(dev, ret, "failed to register watchdog\n");
	}

	ret = devm_add_action_or_reset(dev, zx279133_wdt_stop_action, wdt);
	if (ret)
		return ret;

	dev_info(dev,
		 "running watchdog at %lu Hz, timeout %u s, reset %#x:%#x\n",
		 wdt->count_rate, wdt->wdd.timeout, reset_offset, reset_mask);

	return 0;
}

static const struct of_device_id zx279133_wdt_of_match[] = {
	{ .compatible = "zte,zx279133-watchdog" },
	{ }
};
MODULE_DEVICE_TABLE(of, zx279133_wdt_of_match);

static struct platform_driver zx279133_wdt_driver = {
	.probe = zx279133_wdt_probe,
	.driver = {
		.name = "zx279133-watchdog",
		.of_match_table = zx279133_wdt_of_match,
	},
};
module_platform_driver(zx279133_wdt_driver);

MODULE_DESCRIPTION("ZTE ZX279133 watchdog driver");
MODULE_LICENSE("GPL");
