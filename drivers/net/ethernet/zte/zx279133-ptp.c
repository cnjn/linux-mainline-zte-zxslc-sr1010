// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/module.h>

#include "zx279133.h"

static u64 zx279133_ptp_read_time(struct zx279133_eth *eth)
{
	u32 sec0, sec1, nsec;

	sec1 = readl_relaxed(eth->base + ZX279133_PTP_TOD_SEC);
	do {
		sec0 = sec1;
		nsec = readl_relaxed(eth->base + ZX279133_PTP_TOD_NS);
		sec1 = readl_relaxed(eth->base + ZX279133_PTP_TOD_SEC);
	} while (sec0 != sec1);

	return (u64)sec1 * NSEC_PER_SEC + nsec;
}

static void zx279133_ptp_issue_update(struct zx279133_eth *eth,
				      u32 sec, u32 nsec)
{
	u32 value;

	writel(sec, eth->base + ZX279133_PTP_UPDATE_SEC);
	writel(nsec, eth->base + ZX279133_PTP_UPDATE_NS);

	value = readl(eth->base + ZX279133_PTP_UPDATE);
	value &= ~(ZX279133_PTP_UPDATE_SIGN |
		   ZX279133_PTP_UPDATE_PULSE |
		   ZX279133_PTP_UPDATE_STATUS);
	writel(value | ZX279133_PTP_UPDATE_STATUS,
	       eth->base + ZX279133_PTP_UPDATE);
	writel(value | ZX279133_PTP_UPDATE_STATUS |
	       ZX279133_PTP_UPDATE_PULSE,
	       eth->base + ZX279133_PTP_UPDATE);
	/* The update pulse crosses into the serial PPS/TOD clock domain. */
	usleep_range(100000, 110000);
	writel(value | ZX279133_PTP_UPDATE_STATUS,
	       eth->base + ZX279133_PTP_UPDATE);
}

static int zx279133_ptp_gettimex64(struct ptp_clock_info *info,
				   struct timespec64 *ts,
				   struct ptp_system_timestamp *sts)
{
	struct zx279133_eth *eth = container_of(info, struct zx279133_eth,
					       ptp_info);
	unsigned long flags;
	u64 ns;

	spin_lock_irqsave(&eth->ptp_lock, flags);
	if (!eth->ptp_agc_prepared) {
		spin_unlock_irqrestore(&eth->ptp_lock, flags);
		return -ENETDOWN;
	}
	ptp_read_system_prets(sts);
	ns = zx279133_ptp_read_time(eth);
	ptp_read_system_postts(sts);
	spin_unlock_irqrestore(&eth->ptp_lock, flags);

	*ts = ns_to_timespec64(ns);
	return 0;
}

static int zx279133_ptp_settime64(struct ptp_clock_info *info,
				  const struct timespec64 *ts)
{
	struct zx279133_eth *eth = container_of(info, struct zx279133_eth,
					       ptp_info);

	if (ts->tv_sec < 0 || ts->tv_sec > U32_MAX)
		return -ERANGE;

	mutex_lock(&eth->ptp_cmd_lock);
	if (!eth->ptp_agc_prepared) {
		mutex_unlock(&eth->ptp_cmd_lock);
		return -ENETDOWN;
	}
	/* Hardware loads the staged value on its next serial TOD frame. */
	zx279133_ptp_issue_update(eth, ts->tv_sec, ts->tv_nsec);
	mutex_unlock(&eth->ptp_cmd_lock);

	return 0;
}

static const struct ptp_clock_info zx279133_ptp_info = {
	.owner		= THIS_MODULE,
	.name		= "zx279133-tod",
	/* The recovered compare register does not trim the TOD oscillator. */
	.max_adj	= 0,
	.gettimex64	= zx279133_ptp_gettimex64,
	.settime64	= zx279133_ptp_settime64,
};

void zx279133_ptp_start(struct zx279133_eth *eth)
{
	unsigned long flags;
	u32 value;

	mutex_lock(&eth->ptp_cmd_lock);
	spin_lock_irqsave(&eth->ptp_lock, flags);
	if (!eth->ptp_agc_prepared) {
		eth->ptp_agc_saved = readl(eth->base + ZX279133_PTP_AGC_CFG);
		writel(eth->ptp_agc_saved | ZX279133_PTP_AGC_WORK_CLOCKS,
		       eth->base + ZX279133_PTP_AGC_CFG);
		eth->ptp_agc_prepared = true;
	}

	value = readl(eth->base + ZX279133_PTP_COMPARE);
	value &= ZX279133_PTP_COMPARE_MODE;
	value |= FIELD_PREP(ZX279133_PTP_COMPARE_TIME,
			    ZX279133_PTP_NOMINAL_COMPARE);
	writel(value, eth->base + ZX279133_PTP_COMPARE);

	value = readl(eth->base + ZX279133_PTP_CTRL);
	writel(value | ZX279133_PTP_UPDATE_ENABLE,
	       eth->base + ZX279133_PTP_CTRL);
	spin_unlock_irqrestore(&eth->ptp_lock, flags);
	mutex_unlock(&eth->ptp_cmd_lock);
}

void zx279133_ptp_stop(struct zx279133_eth *eth)
{
	unsigned long flags;

	mutex_lock(&eth->ptp_cmd_lock);
	spin_lock_irqsave(&eth->ptp_lock, flags);
	if (eth->ptp_agc_prepared) {
		writel(eth->ptp_agc_saved,
		       eth->base + ZX279133_PTP_AGC_CFG);
		eth->ptp_agc_prepared = false;
	}
	spin_unlock_irqrestore(&eth->ptp_lock, flags);
	mutex_unlock(&eth->ptp_cmd_lock);
}

static void zx279133_ptp_unregister(void *data)
{
	struct zx279133_eth *eth = data;

	ptp_clock_unregister(eth->ptp_clock);
}

int zx279133_ptp_init(struct zx279133_eth *eth)
{
	int ret;

	eth->ptp_info = zx279133_ptp_info;
	eth->ptp_clock = ptp_clock_register(&eth->ptp_info, eth->dev);
	if (IS_ERR(eth->ptp_clock))
		return PTR_ERR(eth->ptp_clock);

	ret = devm_add_action_or_reset(eth->dev, zx279133_ptp_unregister, eth);
	if (ret)
		return ret;

	return 0;
}

int zx279133_get_ts_info(struct net_device *ndev,
			 struct kernel_ethtool_ts_info *info)
{
	struct zx279133_eth *eth = netdev_priv(ndev);

	ethtool_op_get_ts_info(ndev, info);
	info->phc_index = ptp_clock_index(eth->ptp_clock);
	/* XMAC advertises an external source that is not wired on SR1010. */

	return 0;
}
