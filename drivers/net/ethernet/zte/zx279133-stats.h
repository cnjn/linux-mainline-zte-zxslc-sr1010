/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ZX279133_STATS_H
#define __ZX279133_STATS_H

#include <linux/atomic.h>

struct zx279133_netdev_stats {
	atomic64_t rx_errors;
	atomic64_t tx_errors;
	atomic64_t rx_dropped;
	atomic64_t tx_dropped;
	atomic64_t rx_length_errors;
};

static inline void
zx279133_netdev_stats_init(struct zx279133_netdev_stats *stats)
{
	atomic64_set(&stats->rx_errors, 0);
	atomic64_set(&stats->tx_errors, 0);
	atomic64_set(&stats->rx_dropped, 0);
	atomic64_set(&stats->tx_dropped, 0);
	atomic64_set(&stats->rx_length_errors, 0);
}

#endif /* __ZX279133_STATS_H */
