// SPDX-License-Identifier: GPL-2.0-only
/* Read-only, trigger-once audit of firmware-configured ZX279133 blocks. */

#include <linux/atomic.h>
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define ZX279133_XPCS0_BASE	0x1a000000
#define ZX279133_XPCS1_BASE	0x1b000000
#define ZX279133_XPCS_SIZE	0x00200000
#define ZX279133_NPPT_BASE	0x19000000
#define ZX279133_NPPT_SIZE	0x00001000

#define ZX279133_XPCS_PCLK_RATE	125000000
#define ZX279133_NPPT_WCLK_RATE	416666666

#define ZX279133_NPPT_SHADOW84	0x84
#define ZX279133_NPPT_SHADOW90	0x90
#define ZX279133_NPPT_SHADOW94	0x94

struct zx279133_xpcs_snapshot {
	u16 pma_id[2];
	u16 pcs_id[2];
	u16 an_id[2];
	u16 pma_devs[2];
	u16 dig_sts;
};

struct zx279133_audit_snapshot {
	struct zx279133_xpcs_snapshot xpcs[2];
	u32 nppt[3];
};

enum zx279133_audit_state {
	ZX279133_AUDIT_IDLE,
	ZX279133_AUDIT_SUCCESS,
	ZX279133_AUDIT_FAILURE,
};

struct zx279133_audit {
	struct device *dev;
	struct clk *xpcs_pclk;
	struct clk *nppt_wclk;
	void __iomem *xpcs[2];
	void __iomem *nppt;
	/* Serializes the trigger and cached result access. */
	struct mutex lock;
	enum zx279133_audit_state state;
	int error;
	struct zx279133_audit_snapshot snapshot;
};

/*
 * A permanent claim prevents a second target read after removal and reprobe;
 * the cached result is valid only while the original device exists.
 */
static atomic_t zx279133_audit_claimed = ATOMIC_INIT(0);

static int zx279133_audit_clocks_valid(struct zx279133_audit *audit)
{
	if (clk_get_rate(audit->xpcs_pclk) != ZX279133_XPCS_PCLK_RATE ||
	    clk_get_rate(audit->nppt_wclk) != ZX279133_NPPT_WCLK_RATE)
		return -EINVAL;

	/* No stable accessor exists for the framework prepare count. */
	if (__clk_get_enable_count(audit->xpcs_pclk) ||
	    __clk_get_enable_count(audit->nppt_wclk))
		return -EBUSY;

	if (!__clk_is_enabled(audit->xpcs_pclk) ||
	    !__clk_is_enabled(audit->nppt_wclk))
		return -EBUSY;

	return 0;
}

static int zx279133_xpcs_read_stable(void __iomem *base, u16 devad, u16 reg,
				     u16 *value)
{
	u32 offset = ((((u32)devad << 16) | reg) * 4);
	u32 first, second;

	first = readl(base + offset);
	second = readl(base + offset);
	if (first != second)
		return -EIO;
	if (first & GENMASK(31, 16))
		return -ERANGE;

	*value = first;
	return 0;
}

static int zx279133_nppt_read_stable(void __iomem *base, u32 offset, u32 *value)
{
	u32 first, second;

	first = readl(base + offset);
	second = readl(base + offset);
	if (first != second)
		return -EIO;

	*value = first;
	return 0;
}

static int zx279133_read_xpcs(struct zx279133_audit *audit, unsigned int id)
{
	struct zx279133_xpcs_snapshot *snapshot = &audit->snapshot.xpcs[id];
	void __iomem *base = audit->xpcs[id];
	int ret;

	ret = zx279133_xpcs_read_stable(base, 1, 2, &snapshot->pma_id[0]);
	if (ret)
		return ret;
	ret = zx279133_xpcs_read_stable(base, 1, 3, &snapshot->pma_id[1]);
	if (ret)
		return ret;
	ret = zx279133_xpcs_read_stable(base, 3, 2, &snapshot->pcs_id[0]);
	if (ret)
		return ret;
	ret = zx279133_xpcs_read_stable(base, 3, 3, &snapshot->pcs_id[1]);
	if (ret)
		return ret;
	ret = zx279133_xpcs_read_stable(base, 7, 2, &snapshot->an_id[0]);
	if (ret)
		return ret;
	ret = zx279133_xpcs_read_stable(base, 7, 3, &snapshot->an_id[1]);
	if (ret)
		return ret;
	ret = zx279133_xpcs_read_stable(base, 1, 5, &snapshot->pma_devs[0]);
	if (ret)
		return ret;
	ret = zx279133_xpcs_read_stable(base, 1, 6, &snapshot->pma_devs[1]);
	if (ret)
		return ret;

	return zx279133_xpcs_read_stable(base, 3, 0x8010,
					   &snapshot->dig_sts);
}

static int zx279133_take_snapshot(struct zx279133_audit *audit)
{
	int ret;

	if (atomic_cmpxchg(&zx279133_audit_claimed, 0, 1))
		return -EBUSY;

	ret = zx279133_audit_clocks_valid(audit);
	if (ret)
		return ret;

	ret = zx279133_read_xpcs(audit, 0);
	if (ret)
		return ret;
	ret = zx279133_read_xpcs(audit, 1);
	if (ret)
		return ret;
	ret = zx279133_nppt_read_stable(audit->nppt, ZX279133_NPPT_SHADOW84,
					&audit->snapshot.nppt[0]);
	if (ret)
		return ret;
	ret = zx279133_nppt_read_stable(audit->nppt, ZX279133_NPPT_SHADOW90,
					&audit->snapshot.nppt[1]);
	if (ret)
		return ret;
	ret = zx279133_nppt_read_stable(audit->nppt, ZX279133_NPPT_SHADOW94,
					&audit->snapshot.nppt[2]);
	if (ret)
		return ret;

	return zx279133_audit_clocks_valid(audit);
}

static ssize_t snapshot_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct zx279133_audit *audit = dev_get_drvdata(dev);
	struct zx279133_xpcs_snapshot *xpcs0 = &audit->snapshot.xpcs[0];
	struct zx279133_xpcs_snapshot *xpcs1 = &audit->snapshot.xpcs[1];
	u32 pseq0, pseq1;
	int ret;

	mutex_lock(&audit->lock);
	if (audit->state == ZX279133_AUDIT_IDLE) {
		ret = zx279133_take_snapshot(audit);
		if (ret) {
			audit->error = ret;
			audit->state = ZX279133_AUDIT_FAILURE;
		} else {
			audit->state = ZX279133_AUDIT_SUCCESS;
			dev_info(audit->dev,
				 "snapshot captured: data_reads=42 target_writes=0\n");
		}
	}

	if (audit->state == ZX279133_AUDIT_FAILURE) {
		ret = audit->error;
		goto out_unlock;
	}

	pseq0 = FIELD_GET(GENMASK(4, 2), xpcs0->dig_sts);
	pseq1 = FIELD_GET(GENMASK(4, 2), xpcs1->dig_sts);
	ret = sysfs_emit(buf,
			 "xpcs0 base=0x1a000000 pma_id=%04x:%04x pcs_id=%04x:%04x an_id=%04x:%04x pma_devs=%04x:%04x dig_sts=0x%04x pseq=%u power_good=%c samples=2\n"
			 "xpcs1 base=0x1b000000 pma_id=%04x:%04x pcs_id=%04x:%04x an_id=%04x:%04x pma_devs=%04x:%04x dig_sts=0x%04x pseq=%u power_good=%c samples=2\n"
			 "nppt base=0x19000000 shadow84=0x%08x shadow90=0x%08x shadow94=0x%08x samples=2\n"
			 "summary data_reads=42 target_writes=0 xpcs_pclk=125000000:Y nppt_wclk=416666666:Y mode=read-only\n",
			 xpcs0->pma_id[0], xpcs0->pma_id[1],
			 xpcs0->pcs_id[0], xpcs0->pcs_id[1],
			 xpcs0->an_id[0], xpcs0->an_id[1],
			 xpcs0->pma_devs[0], xpcs0->pma_devs[1],
			 xpcs0->dig_sts, pseq0, pseq0 == 4 ? 'Y' : 'N',
			 xpcs1->pma_id[0], xpcs1->pma_id[1],
			 xpcs1->pcs_id[0], xpcs1->pcs_id[1],
			 xpcs1->an_id[0], xpcs1->an_id[1],
			 xpcs1->pma_devs[0], xpcs1->pma_devs[1],
			 xpcs1->dig_sts, pseq1, pseq1 == 4 ? 'Y' : 'N',
			 audit->snapshot.nppt[0], audit->snapshot.nppt[1],
			 audit->snapshot.nppt[2]);

out_unlock:
	mutex_unlock(&audit->lock);
	return ret;
}
static DEVICE_ATTR_RO(snapshot);

static struct attribute *zx279133_audit_attrs[] = {
	&dev_attr_snapshot.attr,
	NULL,
};

static const struct attribute_group zx279133_audit_group = {
	.attrs = zx279133_audit_attrs,
};

static void __iomem *zx279133_map_resource(struct platform_device *pdev,
					   const char *name,
					   resource_size_t start,
					   resource_size_t size)
{
	struct resource *resource;

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!resource)
		return ERR_PTR(-EINVAL);
	if (resource->start != start || resource_size(resource) != size) {
		dev_err(&pdev->dev, "invalid %s resource\n", name);
		return ERR_PTR(-EINVAL);
	}

	return devm_ioremap_resource(&pdev->dev, resource);
}

static int zx279133_audit_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zx279133_audit *audit;
	int ret;

	audit = devm_kzalloc(dev, sizeof(*audit), GFP_KERNEL);
	if (!audit)
		return -ENOMEM;

	audit->dev = dev;
	audit->xpcs_pclk = devm_clk_get(dev, "xpcs-pclk");
	if (IS_ERR(audit->xpcs_pclk))
		return dev_err_probe(dev, PTR_ERR(audit->xpcs_pclk),
				     "failed to get XPCS PCLK\n");
	audit->nppt_wclk = devm_clk_get(dev, "nppt-wclk");
	if (IS_ERR(audit->nppt_wclk))
		return dev_err_probe(dev, PTR_ERR(audit->nppt_wclk),
				     "failed to get NPPT WCLK\n");

	audit->xpcs[0] = zx279133_map_resource(pdev, "xpcs0",
					       ZX279133_XPCS0_BASE,
					       ZX279133_XPCS_SIZE);
	if (IS_ERR(audit->xpcs[0]))
		return PTR_ERR(audit->xpcs[0]);
	audit->xpcs[1] = zx279133_map_resource(pdev, "xpcs1",
					       ZX279133_XPCS1_BASE,
					       ZX279133_XPCS_SIZE);
	if (IS_ERR(audit->xpcs[1]))
		return PTR_ERR(audit->xpcs[1]);
	audit->nppt = zx279133_map_resource(pdev, "nppt", ZX279133_NPPT_BASE,
					    ZX279133_NPPT_SIZE);
	if (IS_ERR(audit->nppt))
		return PTR_ERR(audit->nppt);

	mutex_init(&audit->lock);
	platform_set_drvdata(pdev, audit);

	ret = devm_device_add_group(dev, &zx279133_audit_group);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to add snapshot attribute\n");

	return 0;
}

static const struct of_device_id zx279133_audit_of_match[] = {
	{ .compatible = "zte,zx279133-xpcs-nppt-audit" },
	{ }
};

static struct platform_driver zx279133_audit_driver = {
	.probe = zx279133_audit_probe,
	.driver = {
		.name = "zx279133-xpcs-nppt-audit",
		.of_match_table = zx279133_audit_of_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(zx279133_audit_driver);
