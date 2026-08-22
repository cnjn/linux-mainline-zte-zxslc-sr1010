// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>

#include "zx279133.h"

static int zx279133_np_reset_prepare(struct zx279133_eth *eth)
{
	u32 value;
	int ret;

	value = readl(eth->base + ZX279133_SYS_SOFT_RESET);
	value &= ~ZX279133_SYS_SDET_RESET_N;
	writel(value, eth->base + ZX279133_SYS_SOFT_RESET);
	usleep_range(400, 450);

	value &= ~ZX279133_SYS_PON_RESET_N;
	writel(value, eth->base + ZX279133_SYS_SOFT_RESET);
	usleep_range(400, 450);
	value |= ZX279133_SYS_PON_RESET_N;
	writel(value, eth->base + ZX279133_SYS_SOFT_RESET);

	eth->sipc_saved[0] = readl(eth->base + ZX279133_SIPC_CFG);
	eth->sipc_saved[1] = readl(eth->base + ZX279133_SIPC_RX_GAP);
	eth->np_reset_prepared = true;
	writel((eth->sipc_saved[0] & ~ZX279133_SIPC_RX_SPA_MASK) |
	       ZX279133_SIPC_RX_SPA_VALUE, eth->base + ZX279133_SIPC_CFG);
	writel(ZX279133_SIPC_RX_GAP_VALUE, eth->base + ZX279133_SIPC_RX_GAP);

	ret = readl_poll_timeout_atomic(eth->base + ZX279133_GLOBAL_INIT_DONE,
					value,
					(value & ZX279133_GLOBAL_INIT_DONE_MASK) ==
					ZX279133_GLOBAL_INIT_DONE_MASK,
					100, 40000);
	usleep_range(200, 250);
	value = readl(eth->base + ZX279133_SYS_SOFT_RESET);
	writel(value | ZX279133_SYS_SDET_RESET_N,
	       eth->base + ZX279133_SYS_SOFT_RESET);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "NPPT global initialization timed out: %#x\n",
				     value);

	return 0;
}

static void zx279133_np_reset_restore(struct zx279133_eth *eth)
{
	if (!eth->np_reset_prepared)
		return;

	writel(eth->sipc_saved[0], eth->base + ZX279133_SIPC_CFG);
	writel(eth->sipc_saved[1], eth->base + ZX279133_SIPC_RX_GAP);
	eth->np_reset_prepared = false;
}

static const u32 zx279133_greg_offsets[] = {
	ZX279133_GREG_BUFFER_SIZE,
	ZX279133_GREG_BUFFER_USABLE,
	ZX279133_GREG_RED_BUFFER_USABLE,
	0x0034,
	0x0038,
	0x003c,
	0x0040,
	0x004c,
	0x00d0,
	0x00d4,
};

static void zx279133_greg_prepare(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_greg_offsets); i++)
		eth->greg_saved[i] = readl(eth->base + zx279133_greg_offsets[i]);

	writel(ZX279133_GREG_BUFFER_SIZE_VALUE,
	       eth->base + ZX279133_GREG_BUFFER_SIZE);
	writel(ZX279133_GREG_BUFFER_USABLE_VALUE,
	       eth->base + ZX279133_GREG_BUFFER_USABLE);
	writel(ZX279133_GREG_BUFFER_USABLE_VALUE,
	       eth->base + ZX279133_GREG_RED_BUFFER_USABLE);
	for (i = 3; i < 7; i++)
		writel(eth->greg_saved[i] | ZX279133_GREG_SMAC_RUNT_MASK,
		       eth->base + zx279133_greg_offsets[i]);
	writel(eth->greg_saved[7] | ZX279133_GREG_SMAC6_RUNT_MASK,
	       eth->base + zx279133_greg_offsets[7]);
	for (i = 8; i < ARRAY_SIZE(zx279133_greg_offsets); i++)
		writel(eth->greg_saved[i] | ZX279133_GREG_XMAC_RUNT_MASK,
		       eth->base + zx279133_greg_offsets[i]);
	eth->greg_prepared = true;
}

static void zx279133_greg_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->greg_prepared)
		return;

	for (i = 0; i < ARRAY_SIZE(zx279133_greg_offsets); i++)
		writel(eth->greg_saved[i], eth->base + zx279133_greg_offsets[i]);
	eth->greg_prepared = false;
}

static const u32 zx279133_dma_offsets[] = {
	ZX279133_DMA_AXI_MODE,
	ZX279133_DMA_AXI_RID0,
	ZX279133_DMA_AXI_RID1,
	ZX279133_DMA_AXI_WID,
	ZX279133_DMA_RDSE_GAP,
};

static void zx279133_dma_prepare(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_dma_offsets); i++)
		eth->dma_saved[i] = readl(eth->base + zx279133_dma_offsets[i]);

	writel(eth->dma_saved[0] & ~ZX279133_DMA_AXI_MODE_BIT,
	       eth->base + ZX279133_DMA_AXI_MODE);
	writel(0x000c0b0a, eth->base + ZX279133_DMA_AXI_RID0);
	writel(0x000f0e0d, eth->base + ZX279133_DMA_AXI_RID1);
	writel(0x000e0c0b, eth->base + ZX279133_DMA_AXI_WID);
	writel((eth->dma_saved[4] & ~ZX279133_DMA_RDSE_GAP_MASK) |
	       FIELD_PREP(ZX279133_DMA_RDSE_GAP_MASK, 8),
	       eth->base + ZX279133_DMA_RDSE_GAP);
	eth->dma_prepared = true;
}

static void zx279133_dma_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->dma_prepared)
		return;

	for (i = 0; i < ARRAY_SIZE(zx279133_dma_offsets); i++)
		writel(eth->dma_saved[i], eth->base + zx279133_dma_offsets[i]);
	eth->dma_prepared = false;
}

static const u32 zx279133_bmu_offsets[] = {
	ZX279133_BMU_ENABLE,
	ZX279133_BMU_BPPI_THRESH,
	ZX279133_BMU_BPO_THRESH,
	ZX279133_BMU_BPPE_BASE,
	ZX279133_BMU_JUMBO_BPPE_BASE,
	ZX279133_BMU_DESC_BASE,
	ZX279133_BMU_NORMAL_BASE,
	ZX279133_BMU_JUMBO_BASE,
	ZX279133_BMU_BUFFER_SIZE,
	ZX279133_BMU_NORMAL_CFG,
	ZX279133_BMU_JUMBO_CFG,
	ZX279133_BMU_NORMAL_INDEX_MAX,
	ZX279133_BMU_JUMBO_INDEX_MAX,
	ZX279133_BMU_NORMAL_RLS_CFG,
	ZX279133_BMU_JUMBO_RLS_CFG,
	ZX279133_BMU_NORMAL_COUNT_REG,
	ZX279133_BMU_JUMBO_COUNT_REG,
	ZX279133_BMU_USABLE_SIZE,
};

static void zx279133_bmu_prepare(struct zx279133_eth *eth)
{
	phys_addr_t normal_base = eth->bmu_base + ZX279133_BMU_BPPE_SIZE;
	phys_addr_t jumbo_base = normal_base +
		ZX279133_BMU_NORMAL_COUNT * ZX279133_BMU_NORMAL_SIZE;
	phys_addr_t desc_base = jumbo_base +
		ZX279133_BMU_JUMBO_COUNT * ZX279133_BMU_JUMBO_SIZE;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_bmu_offsets); i++)
		eth->bmu_saved[i] = readl(eth->base + zx279133_bmu_offsets[i]);

	writel(0, eth->base + ZX279133_BMU_ENABLE);
	for (i = 0; i < ZX279133_BMU_NORMAL_COUNT; i++)
		iowrite16be(i, eth->bmu_mem + 2 * i);
	for (i = 0; i < ZX279133_BMU_JUMBO_COUNT; i++)
		iowrite16be(i, eth->bmu_mem + ZX279133_BMU_JUMBO_BPPE_OFFSET +
			    2 * i);
	/* Publish both BPPE tables before enabling the BMU. */
	wmb();

	writel(0x0104c040, eth->base + ZX279133_BMU_BPPI_THRESH);
	writel(0x0104c040, eth->base + ZX279133_BMU_BPO_THRESH);
	writel(lower_32_bits(eth->bmu_base),
	       eth->base + ZX279133_BMU_BPPE_BASE);
	writel(lower_32_bits(eth->bmu_base + ZX279133_BMU_JUMBO_BPPE_OFFSET),
	       eth->base + ZX279133_BMU_JUMBO_BPPE_BASE);
	writel(lower_32_bits(desc_base), eth->base + ZX279133_BMU_DESC_BASE);
	writel(lower_32_bits(normal_base), eth->base + ZX279133_BMU_NORMAL_BASE);
	writel(lower_32_bits(jumbo_base), eth->base + ZX279133_BMU_JUMBO_BASE);
	writel(ZX279133_BMU_NORMAL_SIZE |
	       ZX279133_BMU_JUMBO_SIZE << 16,
	       eth->base + ZX279133_BMU_BUFFER_SIZE);
	writel(ZX279133_BMU_NORMAL_COUNT << 16,
	       eth->base + ZX279133_BMU_NORMAL_CFG);
	writel(ZX279133_BMU_JUMBO_COUNT << 16,
	       eth->base + ZX279133_BMU_JUMBO_CFG);
	writel((ZX279133_BMU_NORMAL_COUNT >> 5) - 1,
	       eth->base + ZX279133_BMU_NORMAL_INDEX_MAX);
	writel((ZX279133_BMU_JUMBO_COUNT >> 5) - 1,
	       eth->base + ZX279133_BMU_JUMBO_INDEX_MAX);
	writel(ZX279133_BMU_NORMAL_COUNT << 16,
	       eth->base + ZX279133_BMU_NORMAL_RLS_CFG);
	writel(ZX279133_BMU_JUMBO_COUNT << 16,
	       eth->base + ZX279133_BMU_JUMBO_RLS_CFG);
	writel(ZX279133_BMU_NORMAL_COUNT,
	       eth->base + ZX279133_BMU_NORMAL_COUNT_REG);
	writel(ZX279133_BMU_JUMBO_COUNT,
	       eth->base + ZX279133_BMU_JUMBO_COUNT_REG);
	writel(ZX279133_GREG_BUFFER_USABLE_VALUE,
	       eth->base + ZX279133_BMU_USABLE_SIZE);
	writel(1, eth->base + ZX279133_BMU_ENABLE);
	writel(0x01058080, eth->base + ZX279133_BMU_BPPI_THRESH);
	eth->bmu_prepared = true;
}

static void zx279133_bmu_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->bmu_prepared)
		return;

	writel(0, eth->base + ZX279133_BMU_ENABLE);
	for (i = 1; i < ARRAY_SIZE(zx279133_bmu_offsets); i++)
		writel(eth->bmu_saved[i], eth->base + zx279133_bmu_offsets[i]);
	writel(eth->bmu_saved[0], eth->base + ZX279133_BMU_ENABLE);
	eth->bmu_prepared = false;
}

static const u32 zx279133_qmg_offsets[] = {
	ZX279133_QMG_WOE_RED_WATERMARK,
	ZX279133_QMG_WATERMARK,
	ZX279133_QMG_DESC_OUT,
	ZX279133_QMG_WOE_OUT,
	ZX279133_QMG_DESC_IN,
	ZX279133_QMG_VR_GAP,
	ZX279133_QMG_RAM_REQ,
};

static int zx279133_qmg_prepare(struct zx279133_eth *eth)
{
	u32 value;
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(zx279133_qmg_offsets); i++)
		eth->qmg_saved[i] = readl(eth->base + zx279133_qmg_offsets[i]);
	eth->qmg_prepared = true;

	writel((eth->qmg_saved[0] & ~ZX279133_QMG_HIGH16_MASK) |
	       FIELD_PREP(ZX279133_QMG_HIGH16_MASK, 0x5e00),
	       eth->base + ZX279133_QMG_WOE_RED_WATERMARK);
	writel(FIELD_PREP(ZX279133_QMG_HIGH16_MASK, 0x2bb8) |
	       FIELD_PREP(ZX279133_QMG_LOW16_MASK, 0x2fa0),
	       eth->base + ZX279133_QMG_WATERMARK);
	writel((eth->qmg_saved[2] &
		~(ZX279133_QMG_ENABLE_BIT | ZX279133_QMG_LOW16_MASK)) |
	       ZX279133_QMG_ENABLE_BIT |
	       FIELD_PREP(ZX279133_QMG_LOW16_MASK, 0x4e00),
	       eth->base + ZX279133_QMG_DESC_OUT);
	writel((eth->qmg_saved[3] &
		~(ZX279133_QMG_ENABLE_BIT | ZX279133_QMG_LOW16_MASK)) |
	       ZX279133_QMG_ENABLE_BIT |
	       FIELD_PREP(ZX279133_QMG_LOW16_MASK, 0x0fa0),
	       eth->base + ZX279133_QMG_WOE_OUT);
	writel(FIELD_PREP(ZX279133_QMG_HIGH16_MASK, 0x4000) |
	       FIELD_PREP(ZX279133_QMG_LOW16_MASK, 0x3000),
	       eth->base + ZX279133_QMG_DESC_IN);
	writel(FIELD_PREP(ZX279133_QMG_VR_TOTAL_MASK, 70) |
	       FIELD_PREP(ZX279133_QMG_VR_UP_MASK, 20) |
	       FIELD_PREP(ZX279133_QMG_VR_DOWN_MASK, 50),
	       eth->base + ZX279133_QMG_VR_GAP);
	writel((eth->qmg_saved[6] & ~ZX279133_QMG_RAM_MASK) |
	       ZX279133_QMG_RAM_MASK,
	       eth->base + ZX279133_QMG_RAM_REQ);

	ret = readl_poll_timeout_atomic(eth->base + ZX279133_QMG_RAM_DONE,
					value,
					(value & ZX279133_QMG_RAM_MASK) ==
					ZX279133_QMG_RAM_MASK,
					1, 1000);
	if (ret)
		dev_err(eth->dev, "QMG RAM initialization timed out: %#x\n",
			value);

	return ret;
}

static void zx279133_qmg_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->qmg_prepared)
		return;

	for (i = 0; i < ARRAY_SIZE(zx279133_qmg_offsets); i++)
		writel(eth->qmg_saved[i], eth->base + zx279133_qmg_offsets[i]);
	eth->qmg_prepared = false;
}

static void zx279133_sopc_prepare(struct zx279133_eth *eth)
{
	eth->sopc_saved[0] = readl(eth->base + ZX279133_SOPC_CRC_PADDING);
	eth->sopc_saved[1] = readl(eth->base + ZX279133_SOPC_FIFO_THRESHOLD);

	writel(eth->sopc_saved[0] & ~ZX279133_SOPC_CRC_PADDING_MASK,
	       eth->base + ZX279133_SOPC_CRC_PADDING);
	writel((eth->sopc_saved[1] & ~ZX279133_SOPC_FIFO_THRESHOLD_MASK) |
	       (ZX279133_SOPC_FIFO_THRESHOLD_VALUE &
		ZX279133_SOPC_FIFO_THRESHOLD_MASK),
	       eth->base + ZX279133_SOPC_FIFO_THRESHOLD);
	eth->sopc_prepared = true;
}

static void zx279133_sopc_restore(struct zx279133_eth *eth)
{
	if (!eth->sopc_prepared)
		return;

	writel(eth->sopc_saved[0], eth->base + ZX279133_SOPC_CRC_PADDING);
	writel(eth->sopc_saved[1], eth->base + ZX279133_SOPC_FIFO_THRESHOLD);
	eth->sopc_prepared = false;
}

static const u32 zx279133_red_offsets[] = {
	ZX279133_RED_CFG,
	ZX279133_RED_IN_SHARE_MAX,
	ZX279133_RED_CPU133_CFG,
	ZX279133_RED_IDM_IN_SHARE_MAX,
	ZX279133_RED_WOE_IN_SHARE_MAX,
	ZX279133_RED_UP_IN_SHARE_MAX,
	ZX279133_RED_DOWN_IN_SHARE_MAX,
	ZX279133_RED_UP_OUT_MAX,
	ZX279133_RED_DOWN_OUT_MAX,
	ZX279133_RED_ALL_OUT_SHARE_MAX,
	ZX279133_RED_WOE_OUT_SHARE_MAX,
	ZX279133_RED_DESC_WATERMARK,
	ZX279133_RED_IDM_DESC_WATERMARK,
};

static const u32 zx279133_red_np1_offsets[] = {
	ZX279133_RED_NP1_Q0_TH,
	ZX279133_RED_NP1_Q1_TH + 0x0,
	ZX279133_RED_NP1_Q1_TH + 0x4,
	ZX279133_RED_NP1_Q1_TH + 0x8,
	ZX279133_RED_NP1_Q1_TH + 0xc,
	ZX279133_RED_NP1_Q1_TH + 0x10,
	ZX279133_RED_NP1_DESC_WM,
	ZX279133_RED_NP1_ISU_TH + 0x00,
	ZX279133_RED_NP1_ISU_TH + 0x04,
	ZX279133_RED_NP1_ISU_TH + 0x08,
	ZX279133_RED_NP1_ISU_TH + 0x0c,
	ZX279133_RED_NP1_ISU_TH + 0x10,
	ZX279133_RED_NP1_ISU_TH + 0x14,
	ZX279133_RED_NP1_ISU_TH + 0x18,
};

/* red_ind_write(): gate the auto-gate, wait idle, command, data words. */
static void zx279133_red_ind_write(struct zx279133_eth *eth, u32 ram_id,
				   u32 addr, const u32 *data, unsigned int count)
{
	u32 gate = readl(eth->base + ZX279133_RED_AUTO_GATE_REG);
	u32 val;
	unsigned int i;

	if (gate & ZX279133_RED_AUTO_GATE_BIT)
		writel(gate & ~ZX279133_RED_AUTO_GATE_BIT,
		       eth->base + ZX279133_RED_AUTO_GATE_REG);

	readl_poll_timeout_atomic(eth->base + ZX279133_RED_IND_DONE, val,
				  val & ZX279133_RED_IND_DONE_BIT, 10, 200);

	writel(addr | FIELD_PREP(ZX279133_RED_IND_RAM_ID_MASK, ram_id),
	       eth->base + ZX279133_RED_IND_CMD);
	for (i = count; i > 0; i--)
		writel(data[i - 1],
		       eth->base + ZX279133_RED_IND_DATA + 4 * (i - 1));

	writel(gate, eth->base + ZX279133_RED_AUTO_GATE_REG);
}

/* red_np1_config(256, 0x100): CPU-facing queue thresholds and watermarks. */
static void zx279133_red_np1_prepare(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_red_np1_offsets); i++)
		eth->red_np1_saved[i] =
			readl(eth->base + zx279133_red_np1_offsets[i]);

	writel((eth->red_np1_saved[0] & ~ZX279133_RED_NP1_TH_MASK) |
	       ZX279133_RED_NP1_TH_VALUE,
	       eth->base + ZX279133_RED_NP1_Q0_TH);
	for (i = 1; i <= 5; i++)
		writel((eth->red_np1_saved[i] & ~ZX279133_RED_NP1_TH_MASK) |
		       ZX279133_RED_NP1_TH_VALUE,
		       eth->base + ZX279133_RED_NP1_Q1_TH + 4 * (i - 1));
	writel((eth->red_np1_saved[6] & ~ZX279133_RED_NP1_DESC_MASK) |
	       ZX279133_RED_NP1_DESC_VALUE,
	       eth->base + ZX279133_RED_NP1_DESC_WM);
	for (i = 7; i < ARRAY_SIZE(zx279133_red_np1_offsets); i++)
		writel((eth->red_np1_saved[i] & ~ZX279133_RED_NP1_TH_MASK) |
		       ZX279133_RED_NP1_ISU_VALUE,
		       eth->base + ZX279133_RED_NP1_ISU_TH + 4 * (i - 7));
}

static void zx279133_red_np1_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_red_np1_offsets); i++)
		writel(eth->red_np1_saved[i],
		       eth->base + zx279133_red_np1_offsets[i]);
}

/*
 * tm_red_buffer_initial(): 401-queue RED buffer configuration through the
 * indirect port. inside_qbuf_133/outside_qbuf_133 hold guard 32 with max
 * 0x834/0xc00 for queues 0-399 and 0x400/0x80 for queue 400; the four-word
 * buffer configuration is identical for every queue.
 */
static void zx279133_red_buffer_prepare(struct zx279133_eth *eth)
{
	static const u32 buffer_cfg[4] = {
		0x80ff3fff, 0x0100ff80, 0x00010200, 0x00000020,
	};
	u32 word;
	unsigned int q;

	for (q = 0; q < ZX279133_RED_QUEUE_COUNT - 1; q++) {
		word = 0x20 | (0x0c00 << 12);
		zx279133_red_ind_write(eth, 0, q, &word, 1);
		word = 0x20 | (0x0834 << 14);
		zx279133_red_ind_write(eth, 2, q, &word, 1);
		zx279133_red_ind_write(eth, 4, q, buffer_cfg, 4);
	}

	q = ZX279133_RED_QUEUE_COUNT - 1;
	word = 0x0080 << 12;
	zx279133_red_ind_write(eth, 0, q, &word, 1);
	word = 0x20 | (0x0400 << 14);
	zx279133_red_ind_write(eth, 2, q, &word, 1);
	zx279133_red_ind_write(eth, 4, q, buffer_cfg, 4);
}

static void zx279133_red_prepare(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_red_offsets); i++)
		eth->red_saved[i] = readl(eth->base + zx279133_red_offsets[i]);

	writel((eth->red_saved[0] & ~ZX279133_RED_CFG_MASK) | 0x7e,
	       eth->base + ZX279133_RED_CFG);
	writel((eth->red_saved[1] & ~ZX279133_RED_SHARE14_MASK) | 0x1400,
	       eth->base + ZX279133_RED_IN_SHARE_MAX);
	writel(0x000a0001, eth->base + ZX279133_RED_CPU133_CFG);
	writel((eth->red_saved[3] & ~ZX279133_RED_SHARE15_MASK) | 0x500,
	       eth->base + ZX279133_RED_IDM_IN_SHARE_MAX);
	writel((eth->red_saved[4] & ~ZX279133_RED_SHARE15_MASK) | 0x500,
	       eth->base + ZX279133_RED_WOE_IN_SHARE_MAX);
	writel((eth->red_saved[5] & ~ZX279133_RED_SHARE14_MASK) | 0x1200,
	       eth->base + ZX279133_RED_UP_IN_SHARE_MAX);
	writel((eth->red_saved[6] & ~ZX279133_RED_SHARE14_MASK) | 0x400,
	       eth->base + ZX279133_RED_DOWN_IN_SHARE_MAX);
	writel((eth->red_saved[7] & ~ZX279133_RED_LOW16_MASK) | 0x1000,
	       eth->base + ZX279133_RED_UP_OUT_MAX);
	writel((eth->red_saved[8] & ~ZX279133_RED_LOW16_MASK) | 0x3000,
	       eth->base + ZX279133_RED_DOWN_OUT_MAX);
	writel((eth->red_saved[9] & ~ZX279133_RED_LOW16_MASK) | 0x4000,
	       eth->base + ZX279133_RED_ALL_OUT_SHARE_MAX);
	writel((eth->red_saved[10] & ~ZX279133_RED_SHARE15_MASK) | 0x3000,
	       eth->base + ZX279133_RED_WOE_OUT_SHARE_MAX);
	writel(0x60002000, eth->base + ZX279133_RED_DESC_WATERMARK);
	writel((eth->red_saved[12] & ~ZX279133_RED_LOW16_MASK) | 0x500,
	       eth->base + ZX279133_RED_IDM_DESC_WATERMARK);
	zx279133_red_np1_prepare(eth);
	zx279133_red_buffer_prepare(eth);
	eth->red_prepared = true;
}

static void zx279133_red_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->red_prepared)
		return;

	for (i = 0; i < ARRAY_SIZE(zx279133_red_offsets); i++)
		writel(eth->red_saved[i], eth->base + zx279133_red_offsets[i]);
	zx279133_red_np1_restore(eth);
	eth->red_prepared = false;
}

static const u32 zx279133_ssch_offsets[] = {
	ZX279133_SSCH_CFG,
	ZX279133_SSCH_SPEND_BYTE,
	ZX279133_SSCH_FILL_TIME,
	ZX279133_SSCH_AGING_TIME,
};

static void zx279133_ssch_prepare(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_ssch_offsets); i++)
		eth->ssch_saved[i] = readl(eth->base + zx279133_ssch_offsets[i]);

	writel((eth->ssch_saved[0] & ~ZX279133_SSCH_CFG_MASK) | 0x3003,
	       eth->base + ZX279133_SSCH_CFG);
	writel((eth->ssch_saved[1] & ~ZX279133_SSCH_SPEND_BYTE_MASK) | 0x814,
	       eth->base + ZX279133_SSCH_SPEND_BYTE);
	writel((eth->ssch_saved[2] & ~ZX279133_SSCH_FILL_TIME_MASK) | 0x400,
	       eth->base + ZX279133_SSCH_FILL_TIME);
	writel(250000000, eth->base + ZX279133_SSCH_AGING_TIME);
	eth->ssch_prepared = true;
}

static void zx279133_ssch_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->ssch_prepared)
		return;

	for (i = 0; i < ARRAY_SIZE(zx279133_ssch_offsets); i++)
		writel(eth->ssch_saved[i], eth->base + zx279133_ssch_offsets[i]);
	eth->ssch_prepared = false;
}

static void zx279133_wsch_prepare(struct zx279133_eth *eth)
{
	eth->wsch_saved[0] = readl(eth->base + ZX279133_WSCH_CFG);
	eth->wsch_saved[1] = readl(eth->base + ZX279133_WSCH_FILL_TIME);

	writel(eth->wsch_saved[0] | ZX279133_WSCH_SHARP_ENABLE,
	       eth->base + ZX279133_WSCH_CFG);
	writel((eth->wsch_saved[1] & ~ZX279133_WSCH_FILL_TIME_MASK) | 0x400,
	       eth->base + ZX279133_WSCH_FILL_TIME);
	eth->wsch_prepared = true;
}

static void zx279133_wsch_restore(struct zx279133_eth *eth)
{
	if (!eth->wsch_prepared)
		return;

	writel(eth->wsch_saved[0], eth->base + ZX279133_WSCH_CFG);
	writel(eth->wsch_saved[1], eth->base + ZX279133_WSCH_FILL_TIME);
	eth->wsch_prepared = false;
}

static void zx279133_woe_sch_prepare(struct zx279133_eth *eth)
{
	eth->woe_sch_saved[0] = readl(eth->base + ZX279133_WOE_SCH_CFG);
	eth->woe_sch_saved[1] = readl(eth->base + ZX279133_WOE_SCH_FILL_TIME);

	writel(eth->woe_sch_saved[0] | ZX279133_WOE_SCH_SHARP_ENABLE,
	       eth->base + ZX279133_WOE_SCH_CFG);
	writel((eth->woe_sch_saved[1] & ~ZX279133_WOE_SCH_FILL_TIME_MASK) |
	       0x400, eth->base + ZX279133_WOE_SCH_FILL_TIME);
	eth->woe_sch_prepared = true;
}

static void zx279133_woe_sch_restore(struct zx279133_eth *eth)
{
	if (!eth->woe_sch_prepared)
		return;

	writel(eth->woe_sch_saved[0], eth->base + ZX279133_WOE_SCH_CFG);
	writel(eth->woe_sch_saved[1], eth->base + ZX279133_WOE_SCH_FILL_TIME);
	eth->woe_sch_prepared = false;
}

static void zx279133_usch_prepare(struct zx279133_eth *eth)
{
	eth->usch_saved[0] = readl(eth->base + ZX279133_USCH_CFG);
	eth->usch_saved[1] = readl(eth->base + ZX279133_USCH_AGING_TIME);

	writel(eth->usch_saved[0] | ZX279133_USCH_CFG_MASK,
	       eth->base + ZX279133_USCH_CFG);
	writel(250000000, eth->base + ZX279133_USCH_AGING_TIME);
	eth->usch_prepared = true;
}

static void zx279133_usch_restore(struct zx279133_eth *eth)
{
	if (!eth->usch_prepared)
		return;

	writel(eth->usch_saved[0], eth->base + ZX279133_USCH_CFG);
	writel(eth->usch_saved[1], eth->base + ZX279133_USCH_AGING_TIME);
	eth->usch_prepared = false;
}

static const u32 zx279133_uopc_offsets[] = {
	ZX279133_UOPC_BURST_NUM,
	ZX279133_UOPC_FIFO_DEPTH,
	ZX279133_UOPC_FIFO_ENABLE0,
	ZX279133_UOPC_FIFO_ENABLE1,
	ZX279133_UOPC_FIFO_SELECT0,
	ZX279133_UOPC_FIFO_SELECT1,
	ZX279133_UOPC_FIFO_BASE0,
	ZX279133_UOPC_FIFO_BASE1,
	ZX279133_UOPC_FIFO_BASE2,
	ZX279133_UOPC_FIFO_BASE3,
	ZX279133_UOPC_FIFO_BASE4,
	ZX279133_UOPC_INFO_AFULL_GAP,
	ZX279133_UOPC_PRE_AFULL_GAP0,
	ZX279133_UOPC_PRE_AFULL_GAP1,
};

static int zx279133_uopc_command(struct zx279133_eth *eth, bool set_mode)
{
	u32 value;
	int ret;

	ret = readl_poll_timeout_atomic(eth->base + ZX279133_UOPC_TCONT_CMD,
					value,
					!(value & ZX279133_UOPC_TCONT_CMD_BIT),
					1, 1000);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "UOPC tcont command remained busy: %#x\n",
				     value);

	if (set_mode)
		value &= ~ZX279133_UOPC_TCONT_MODE_MASK;
	writel(value | ZX279133_UOPC_TCONT_CMD_BIT,
	       eth->base + ZX279133_UOPC_TCONT_CMD);

	ret = readl_poll_timeout_atomic(eth->base + ZX279133_UOPC_TCONT_CMD,
					value,
					!(value & ZX279133_UOPC_TCONT_CMD_BIT),
					1, 1000);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "UOPC tcont command timed out: %#x\n",
				     value);

	return 0;
}

static int zx279133_uopc_prepare(struct zx279133_eth *eth)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(zx279133_uopc_offsets); i++)
		eth->uopc_saved[i] = readl(eth->base + zx279133_uopc_offsets[i]);
	eth->uopc_prepared = true;

	ret = zx279133_uopc_command(eth, true);
	if (ret)
		return ret;

	writel((eth->uopc_saved[0] & ZX279133_UOPC_HIGH8_MASK) | 0x00060606,
	       eth->base + ZX279133_UOPC_BURST_NUM);
	writel((eth->uopc_saved[1] & ZX279133_UOPC_HIGH8_MASK) | 0x00090a08,
	       eth->base + ZX279133_UOPC_FIFO_DEPTH);
	writel(0x0001ffff, eth->base + ZX279133_UOPC_FIFO_ENABLE0);
	writel(eth->uopc_saved[3] & ~ZX279133_UOPC_LOW8_MASK,
	       eth->base + ZX279133_UOPC_FIFO_ENABLE1);
	writel(0xaaa95554, eth->base + ZX279133_UOPC_FIFO_SELECT0);
	writel((eth->uopc_saved[5] & ~ZX279133_UOPC_LOW2_MASK) | 2,
	       eth->base + ZX279133_UOPC_FIFO_SELECT1);
	writel(0x1c120800, eth->base + ZX279133_UOPC_FIFO_BASE0);
	writel(0x443a3026, eth->base + ZX279133_UOPC_FIFO_BASE1);
	writel(0x6a61584e, eth->base + ZX279133_UOPC_FIFO_BASE2);
	writel(0x8e857c73, eth->base + ZX279133_UOPC_FIFO_BASE3);
	writel((eth->uopc_saved[10] & ~ZX279133_UOPC_LOW8_MASK) | 0x97,
	       eth->base + ZX279133_UOPC_FIFO_BASE4);
	writel((eth->uopc_saved[11] & ~ZX279133_UOPC_INFO_GAP_MASK) | 10,
	       eth->base + ZX279133_UOPC_INFO_AFULL_GAP);
	writel((eth->uopc_saved[12] & ~ZX279133_UOPC_PRE_GAP_MASK) |
	       0x00110011, eth->base + ZX279133_UOPC_PRE_AFULL_GAP0);
	writel((eth->uopc_saved[13] & ~ZX279133_UOPC_LOW12_MASK) | 17,
	       eth->base + ZX279133_UOPC_PRE_AFULL_GAP1);

	return zx279133_uopc_command(eth, false);
}

static void zx279133_uopc_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->uopc_prepared)
		return;

	for (i = 0; i < ARRAY_SIZE(zx279133_uopc_offsets); i++)
		writel(eth->uopc_saved[i], eth->base + zx279133_uopc_offsets[i]);
	eth->uopc_prepared = false;
}

static int zx279133_se_frontend_prepare(struct zx279133_eth *eth)
{
	u32 value;
	int ret;

	ret = readl_poll_timeout_atomic(eth->pps_base +
					ZX279133_SE_PARSER_RAM_DONE,
					value,
					(value & ZX279133_SE_PARSER_RAM_MASK) ==
					ZX279133_SE_PARSER_RAM_MASK,
					1, 1000);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "SE parser RAM initialization timed out: %#x\n",
				     value);

	ret = readl_poll_timeout_atomic(eth->pps_base +
					ZX279133_SE_SMMU0_RAM_DONE,
					value,
					value & ZX279133_SE_SMMU0_RAM_MASK,
					1, 1000);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "SE SMMU0 RAM initialization timed out: %#x\n",
				     value);

	eth->se_parser_debug_saved = readl(eth->pps_base +
					 ZX279133_SE_PARSER_DEBUG_CFG);
	eth->se_frontend_prepared = true;
	writel((eth->se_parser_debug_saved & ~ZX279133_SE_PARSER_DEBUG_MASK) | 1,
	       eth->pps_base + ZX279133_SE_PARSER_DEBUG_CFG);

	return 0;
}

static void zx279133_se_frontend_restore(struct zx279133_eth *eth)
{
	if (!eth->se_frontend_prepared)
		return;

	writel(eth->se_parser_debug_saved,
	       eth->pps_base + ZX279133_SE_PARSER_DEBUG_CFG);
	eth->se_frontend_prepared = false;
}

/*
 * se_hash_ddr_init() in np.ko assigns the complete 16 MiB hash window to
 * all eight CPU133 hash bulks.  The vendor indirect helpers reduce the
 * physical base to 4 KiB units, select 512-bit tables, and use depth 18 for
 * this window.  These are PPS registers, not NPPT registers.
 */
static int zx279133_se_hash_prepare(struct zx279133_eth *eth)
{
	u32 base = (u32)(eth->se_hash_base >> 12);
	u32 crc, bulk, depth0, depth1;
	unsigned int i;

	if (eth->se_hash_size < ZX279133_SE_HASH_REQUIRED_SIZE)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(eth->se_hash_smmu1_saved); i++)
		eth->se_hash_smmu1_saved[i] = readl(eth->pps_base +
						    ZX279133_SE_SMMU1_HASH_BASE +
						    i * sizeof(u32));
	eth->se_hash_alg_crc_saved = readl(eth->pps_base +
						 ZX279133_SE_ALG_HASH_CRC);
	eth->se_hash_alg_bulk_saved = readl(eth->pps_base +
						  ZX279133_SE_ALG_HASH_BULK);
	eth->se_hash_alg_depth_saved[0] = readl(eth->pps_base +
						       ZX279133_SE_ALG_HASH_DEPTH0);
	eth->se_hash_alg_depth_saved[1] = readl(eth->pps_base +
						       ZX279133_SE_ALG_HASH_DEPTH1);

	memset_io(eth->se_hash_mem, 0, ZX279133_SE_HASH_REQUIRED_SIZE);
	/* Publish the empty hash window before enabling hardware lookups. */
	wmb();

	for (i = 0; i < ARRAY_SIZE(eth->se_hash_smmu1_saved); i++)
		writel(base, eth->pps_base + ZX279133_SE_SMMU1_HASH_BASE +
		       i * sizeof(u32));

	/* se_alg_set_hash_ext_crc_cfg(1, bulk, 0), bulk 0..7. */
	crc = eth->se_hash_alg_crc_saved & ~GENMASK(15, 0);
	/* se_alg_set_hash_ext_bulk_mode(1, bulk, 512), bulk 0..7. */
	bulk = eth->se_hash_alg_bulk_saved | GENMASK(8, 1);
	/* se_alg_set_hash_ext_depth(1, bulk, 18), bulk 0..7. */
	depth0 = eth->se_hash_alg_depth_saved[0];
	depth1 = eth->se_hash_alg_depth_saved[1];
	for (i = 0; i < 4; i++) {
		depth0 = (depth0 & ~(0xffu << (8 * i))) |
			(18u << (8 * i));
		depth1 = (depth1 & ~(0xffu << (8 * i))) |
			(18u << (8 * i));
	}
	writel(crc, eth->pps_base + ZX279133_SE_ALG_HASH_CRC);
	writel(bulk, eth->pps_base + ZX279133_SE_ALG_HASH_BULK);
	writel(depth0, eth->pps_base + ZX279133_SE_ALG_HASH_DEPTH0);
	writel(depth1, eth->pps_base + ZX279133_SE_ALG_HASH_DEPTH1);
	eth->se_hash_prepared = true;

	return 0;
}

static void zx279133_se_hash_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->se_hash_prepared)
		return;

	for (i = 0; i < ARRAY_SIZE(eth->se_hash_smmu1_saved); i++)
		writel(eth->se_hash_smmu1_saved[i], eth->pps_base +
		       ZX279133_SE_SMMU1_HASH_BASE + i * sizeof(u32));
	writel(eth->se_hash_alg_crc_saved, eth->pps_base +
	       ZX279133_SE_ALG_HASH_CRC);
	writel(eth->se_hash_alg_bulk_saved, eth->pps_base +
	       ZX279133_SE_ALG_HASH_BULK);
	writel(eth->se_hash_alg_depth_saved[0], eth->pps_base +
	       ZX279133_SE_ALG_HASH_DEPTH0);
	writel(eth->se_hash_alg_depth_saved[1], eth->pps_base +
	       ZX279133_SE_ALG_HASH_DEPTH1);
	memset(eth->se_hash_id_map, 0, sizeof(eth->se_hash_id_map));
	eth->se_multi_idx_map = 0;
	eth->se_age_done = false;
	eth->se_hash_prepared = false;
}

int zx279133_tm_prepare(struct zx279133_eth *eth)
{
	int ret;

	ret = zx279133_np_reset_prepare(eth);
	if (ret)
		goto err_np_reset_restore;
	zx279133_greg_prepare(eth);
	zx279133_dma_prepare(eth);
	zx279133_bmu_prepare(eth);
	ret = zx279133_qmg_prepare(eth);
	if (ret)
		goto err_qmg_restore;
	zx279133_sopc_prepare(eth);
	zx279133_red_prepare(eth);
	zx279133_ssch_prepare(eth);
	zx279133_wsch_prepare(eth);
	zx279133_woe_sch_prepare(eth);
	zx279133_usch_prepare(eth);
	ret = zx279133_uopc_prepare(eth);
	if (ret)
		goto err_uopc_restore;
	ret = zx279133_se_frontend_prepare(eth);
	if (ret)
		goto err_se_frontend_restore;
	ret = zx279133_se_hash_prepare(eth);
	if (ret)
		goto err_se_hash_restore;

	return 0;

err_se_hash_restore:
	zx279133_se_frontend_restore(eth);
err_se_frontend_restore:
	zx279133_uopc_restore(eth);
err_uopc_restore:
	zx279133_usch_restore(eth);
	zx279133_woe_sch_restore(eth);
	zx279133_wsch_restore(eth);
	zx279133_ssch_restore(eth);
	zx279133_red_restore(eth);
	zx279133_sopc_restore(eth);
err_qmg_restore:
	zx279133_qmg_restore(eth);
	zx279133_bmu_restore(eth);
	zx279133_dma_restore(eth);
	zx279133_greg_restore(eth);
err_np_reset_restore:
	zx279133_np_reset_restore(eth);
	return ret;
}

void zx279133_tm_restore(struct zx279133_eth *eth)
{
	zx279133_se_hash_restore(eth);
	zx279133_se_frontend_restore(eth);
	zx279133_uopc_restore(eth);
	zx279133_usch_restore(eth);
	zx279133_woe_sch_restore(eth);
	zx279133_wsch_restore(eth);
	zx279133_ssch_restore(eth);
	zx279133_red_restore(eth);
	zx279133_sopc_restore(eth);
	zx279133_qmg_restore(eth);
	zx279133_bmu_restore(eth);
	zx279133_dma_restore(eth);
	zx279133_greg_restore(eth);
	zx279133_np_reset_restore(eth);
}
