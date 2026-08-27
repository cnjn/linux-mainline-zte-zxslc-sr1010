/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ZX279133_H__
#define __ZX279133_H__

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/mdio.h>
#include <linux/netdevice.h>
#include <linux/phylink.h>
#include <linux/workqueue.h>

#include "zx279133-stats.h"
#include "zx279133-lan.h"

#define ZX279133_NUM_CLOCKS	6
#define ZX279133_NUM_IRQS	5

/* Validated WAN/LAN hardware constants. */
#define zx279133_tx_queue		1
#define zx279133_tx_port		6
#define ZX279133_LAN_TX_PORT		5
#define ZX279133_LAN_RX_QUEUE_COUNT	8
#define ZX279133_LAN_TRANSPORT_VID_MIN	59
#define ZX279133_LAN_TRANSPORT_VID_MAX	62
#define ZX279133_LAN_INGRESS_VID	1
#define ZX279133_LAN_VID		62
#define ZX279133_TX_RECLAIM_DELAY_MS	10
#define ZX279133_DATAPATH_USER_WAN	BIT(0)
#define ZX279133_DATAPATH_USER_LAN	BIT(1)
#define zx279133_tx_selector		0x0f
#define zx279133_tx_pon_control		0x08000000
#define zx279133_tx_word4_bit23		0
#define zx279133_tx_hw_csum		1
#define zx279133_tx_in_window		1
#define zx279133_tx_window_offset	0
#define zx279133_tx_bounce_offset	0
#define zx279133_idm_cfg_bit24		0

#define ZX279133_BMU_REQUIRED_SIZE	0x00ecc000
#define ZX279133_SE_HASH_REQUIRED_SIZE	0x01000000
#define ZX279133_BMU_BPPE_SIZE		0x00020000
#define ZX279133_BMU_NORMAL_COUNT	0x00001800
#define ZX279133_BMU_NORMAL_SIZE		0x00000840
#define ZX279133_BMU_JUMBO_COUNT		0x00000020
#define ZX279133_BMU_JUMBO_SIZE		0x00002600
#define ZX279133_BMU_DESC_SIZE		0x00200000
#define ZX279133_BMU_JUMBO_BPPE_OFFSET	0x00010000

#define ZX279133_ROUTE_CLK	0x019c
#define ZX279133_ROUTE_CTRL	0x2438
#define ZX279133_ROUTE_CLK_EN	BIT(0)
#define ZX279133_ROUTE_XMAC1	BIT(2)

#define ZX279133_IDM_CCI_VALUE	0x00200020

#define ZX279133_NP_READY		0x0080
#define ZX279133_NP_READY_MASK		0x01fd

#define ZX279133_GREG_BUFFER_SIZE	0x0068
#define ZX279133_GREG_BUFFER_USABLE	0x006c
#define ZX279133_GREG_RED_BUFFER_USABLE	0x20078
#define ZX279133_GREG_BUFFER_SIZE_VALUE	0x27800940
#define ZX279133_GREG_BUFFER_USABLE_VALUE 0x00000781
#define ZX279133_GREG_SMAC_RUNT_MASK	BIT(18)
#define ZX279133_GREG_SMAC6_RUNT_MASK	BIT(16)
#define ZX279133_GREG_XMAC_RUNT_MASK	0x00000410

#define ZX279133_GLOBAL_INIT_DONE	0x0080
#define ZX279133_GLOBAL_INIT_DONE_MASK	0x000001fd
#define ZX279133_SIPC_CFG		0x4000
#define ZX279133_SIPC_RX_SPA_MASK	GENMASK(2, 0)
#define ZX279133_SIPC_RX_SPA_VALUE	0x3
/* np.ko sipc_set_sch_mode_sw(0): scheduler handoff mode must be zero. */
#define ZX279133_SIPC_SCH_MODE_SW	BIT(2)
#define ZX279133_SIPC_RX_GAP		0x4010
#define ZX279133_SIPC_RX_GAP_VALUE	0x1042
#define ZX279133_SYS_SOFT_RESET		0x2c0004
#define ZX279133_SYS_SDET_RESET_N	BIT(4)
#define ZX279133_SYS_PON_RESET_N		BIT(31)

#define ZX279133_DMA_AXI_MODE		0x38000
#define ZX279133_DMA_AXI_RID0		0x38080
#define ZX279133_DMA_AXI_RID1		0x38084
#define ZX279133_DMA_AXI_WID		0x38088
#define ZX279133_DMA_RDSE_GAP		0x38398
#define ZX279133_DMA_AXI_MODE_BIT	BIT(21)
#define ZX279133_DMA_RDSE_GAP_MASK	GENMASK(14, 10)

#define ZX279133_BMU_ENABLE		0x3c000
#define ZX279133_BMU_BPPI_THRESH		0x3c004
#define ZX279133_BMU_BPO_THRESH		0x3c008
#define ZX279133_BMU_BPPE_BASE		0x00054
#define ZX279133_BMU_JUMBO_BPPE_BASE	0x00058
#define ZX279133_BMU_DESC_BASE		0x0005c
#define ZX279133_BMU_NORMAL_BASE		0x00060
#define ZX279133_BMU_JUMBO_BASE		0x00064
#define ZX279133_BMU_BUFFER_SIZE		0x00068
#define ZX279133_BMU_NORMAL_CFG		0x3c048
#define ZX279133_BMU_JUMBO_CFG		0x3c04c
#define ZX279133_BMU_NORMAL_INDEX_MAX	0x3c058
#define ZX279133_BMU_JUMBO_INDEX_MAX	0x3c05c
#define ZX279133_BMU_NORMAL_RLS_CFG	0x3c060
#define ZX279133_BMU_JUMBO_RLS_CFG	0x3c064
#define ZX279133_BMU_NORMAL_COUNT_REG	0x20030
#define ZX279133_BMU_JUMBO_COUNT_REG	0x20034
#define ZX279133_BMU_USABLE_SIZE		0x20078

#define ZX279133_QMG_RAM_REQ		0x24000
#define ZX279133_QMG_RAM_DONE		0x24004
#define ZX279133_QMG_WATERMARK		0x24014
#define ZX279133_QMG_DESC_OUT		0x24018
#define ZX279133_QMG_WOE_OUT		0x24020
#define ZX279133_QMG_DESC_IN		0x24060
#define ZX279133_QMG_VR_GAP		0x24068
#define ZX279133_QMG_WOE_RED_WATERMARK	0x20088
#define ZX279133_QMG_RAM_MASK		GENMASK(4, 0)
#define ZX279133_QMG_LOW16_MASK		GENMASK(15, 0)
#define ZX279133_QMG_HIGH16_MASK		GENMASK(31, 16)
#define ZX279133_QMG_ENABLE_BIT		BIT(31)
#define ZX279133_QMG_VR_TOTAL_MASK	GENMASK(26, 18)
#define ZX279133_QMG_VR_UP_MASK		GENMASK(17, 9)
#define ZX279133_QMG_VR_DOWN_MASK	GENMASK(8, 0)

#define ZX279133_SOPC_CRC_PADDING	0x343c4
#define ZX279133_SOPC_FIFO_THRESHOLD	0x343d0
#define ZX279133_SOPC_CRC_PADDING_MASK	GENMASK(13, 0)
#define ZX279133_SOPC_FIFO_THRESHOLD_MASK 0x00001f1f
#define ZX279133_SOPC_FIFO_THRESHOLD_VALUE 2056

#define ZX279133_RED_CFG			0x20004
#define ZX279133_RED_IN_SHARE_MAX	0x20040
#define ZX279133_RED_UP_IN_SHARE_MAX	0x20060
#define ZX279133_RED_DOWN_IN_SHARE_MAX	0x20064
#define ZX279133_RED_ALL_OUT_SHARE_MAX	0x2006c
#define ZX279133_RED_DOWN_OUT_MAX	0x20070
#define ZX279133_RED_UP_OUT_MAX		0x20074
#define ZX279133_RED_DESC_WATERMARK	0x20084
#define ZX279133_RED_IDM_DESC_WATERMARK	0x20088
#define ZX279133_RED_CPU133_CFG		0x2024c
#define ZX279133_RED_IDM_IN_SHARE_MAX	0x20344
#define ZX279133_RED_WOE_IN_SHARE_MAX	0x20348
#define ZX279133_RED_WOE_OUT_SHARE_MAX	0x2034c
#define ZX279133_RED_CFG_MASK		GENMASK(7, 0)
#define ZX279133_RED_SHARE14_MASK	GENMASK(13, 0)
#define ZX279133_RED_SHARE15_MASK	GENMASK(14, 0)
#define ZX279133_RED_LOW16_MASK		GENMASK(15, 0)

/*
 * red_np1_config(256, 0x100) register image (NPPT word registers):
 * queue 0 threshold at 0x8004, queues 1-5 at 0x80d5-0x80d9, descriptor
 * watermark at 0x80da, and ISU thresholds at 0x8098-0x809e.
 */
#define ZX279133_RED_NP1_Q0_TH		0x20010
#define ZX279133_RED_NP1_Q1_TH		0x20354
#define ZX279133_RED_NP1_DESC_WM	0x20368
#define ZX279133_RED_NP1_ISU_TH		0x20260
#define ZX279133_RED_NP1_TH_MASK	GENMASK(13, 0)
#define ZX279133_RED_NP1_DESC_MASK	GENMASK(15, 0)
#define ZX279133_RED_NP1_TH_VALUE	272
#define ZX279133_RED_NP1_DESC_VALUE	0x100
#define ZX279133_RED_NP1_ISU_VALUE	15

/*
 * RED indirect access port (red_ind_write): command at 0x20014
 * (addr | ram_id << 22 | rw << 27), done at 0x20018 bit 0, four data
 * words at 0x2001c-0x20028, and the shared auto-gate at 0xb8 bit 10.
 */
#define ZX279133_RED_IND_CMD		0x20014
#define ZX279133_RED_IND_DONE		0x20018
#define ZX279133_RED_IND_DATA		0x2001c
#define ZX279133_RED_IND_DONE_BIT	BIT(0)
#define ZX279133_RED_IND_RAM_ID_MASK	GENMASK(24, 22)
#define ZX279133_RED_AUTO_GATE_REG	0x00b8
#define ZX279133_RED_AUTO_GATE_BIT	BIT(10)
#define ZX279133_RED_QUEUE_COUNT	401

#define ZX279133_SSCH_CFG		0x2c000
#define ZX279133_SSCH_AGING_TIME		0x2c004
#define ZX279133_SSCH_SPEND_BYTE		0x2c024
#define ZX279133_SSCH_FILL_TIME		0x2c028
#define ZX279133_SSCH_CFG_MASK		(BIT(13) | BIT(12) | BIT(1) | BIT(0))
#define ZX279133_SSCH_SPEND_BYTE_MASK	(GENMASK(15, 8) | GENMASK(5, 0))
#define ZX279133_SSCH_FILL_TIME_MASK	GENMASK(17, 0)

#define ZX279133_WSCH_CFG		0x2c400
#define ZX279133_WSCH_FILL_TIME		0x2c444
#define ZX279133_WSCH_SHARP_ENABLE	BIT(1)
#define ZX279133_WSCH_FILL_TIME_MASK	GENMASK(17, 0)

#define ZX279133_WOE_SCH_CFG		0x2c800
#define ZX279133_WOE_SCH_FILL_TIME	0x2c828
#define ZX279133_WOE_SCH_SHARP_ENABLE	BIT(0)
#define ZX279133_WOE_SCH_FILL_TIME_MASK	GENMASK(17, 0)

#define ZX279133_USCH_CFG		0x28000
#define ZX279133_USCH_AGING_TIME		0x28004
#define ZX279133_USCH_CFG_MASK		GENMASK(6, 0)

#define ZX279133_UOPC_TCONT_CMD		0x30000
#define ZX279133_UOPC_BURST_NUM		0x3004c
#define ZX279133_UOPC_FIFO_DEPTH		0x30200
#define ZX279133_UOPC_FIFO_ENABLE0	0x30204
#define ZX279133_UOPC_FIFO_ENABLE1	0x30208
#define ZX279133_UOPC_FIFO_SELECT0	0x3020c
#define ZX279133_UOPC_FIFO_SELECT1	0x30210
#define ZX279133_UOPC_FIFO_BASE0		0x30218
#define ZX279133_UOPC_FIFO_BASE1		0x3021c
#define ZX279133_UOPC_FIFO_BASE2		0x30220
#define ZX279133_UOPC_FIFO_BASE3		0x30224
#define ZX279133_UOPC_FIFO_BASE4		0x30228
#define ZX279133_UOPC_INFO_AFULL_GAP	0x30240
#define ZX279133_UOPC_PRE_AFULL_GAP0	0x30254
#define ZX279133_UOPC_PRE_AFULL_GAP1	0x30258
#define ZX279133_UOPC_TCONT_CMD_BIT	BIT(3)
#define ZX279133_UOPC_TCONT_MODE_MASK	GENMASK(5, 3)
#define ZX279133_UOPC_HIGH8_MASK		GENMASK(31, 24)
#define ZX279133_UOPC_LOW8_MASK		GENMASK(7, 0)
#define ZX279133_UOPC_LOW2_MASK		GENMASK(1, 0)
#define ZX279133_UOPC_INFO_GAP_MASK	GENMASK(4, 0)
#define ZX279133_UOPC_PRE_GAP_MASK	0x0fff0fff
#define ZX279133_UOPC_LOW12_MASK		GENMASK(11, 0)

#define ZX279133_SDET_FRAME_CFG		0x2040
#define ZX279133_SDET_DOWN_FRAME_CFG	0x2044
#define ZX279133_SDET_UP_MAX_LEN	2048
#define ZX279133_SDET_UP_MIN_LEN	12
#define ZX279133_SDET_DOWN_MAX_LEN	2048
#define ZX279133_SDET_DOWN_MIN_LEN	12
#define ZX279133_SDET_UP_MAX_SHIFT	14
#define ZX279133_SDET_DOWN_MAX_SHIFT	16

#define ZX279133_SMCT_SEL_CFG0		0xc000
#define ZX279133_SMCT_SEL_CFG1		0xc004
#define ZX279133_SMCT_CTRL		0xc008
#define ZX279133_SMCT_MC_TH		0xc038
#define ZX279133_SMCT_NP1_TH		0xc05c
#define ZX279133_SMCT_IDM_SSCH_TH	0xc060
#define ZX279133_SMCT_COS_TH0		0xc028
#define ZX279133_SMCT_INPORT_TH0	0xc03c
#define ZX279133_SMCT_SEL_CFG0_VALUE	0x1f00
#define ZX279133_SMCT_SEL_CFG1_VALUE	0x1fff
#define ZX279133_SMCT_CTRL_VALUE	0x9
#define ZX279133_SMCT_COS_TH_VALUE	0x1b00
#define ZX279133_SMCT_COS7_TH_VALUE	0x1d00
#define ZX279133_SMCT_COS_LAST_VALUE	0x1fff
#define ZX279133_SMCT_INPORT_TH_VALUE	0x1c00
#define ZX279133_SMCT_NP1_TH_VALUE	0x1a00
#define ZX279133_SMCT_MC_TH_VALUE	0x1c00
#define ZX279133_SMCT_MC_TH_MASK	GENMASK(13, 0)
#define ZX279133_SMCT_PAIR_PRESERVE_MASK	(GENMASK(31, 30) | GENMASK(15, 14))
#define ZX279133_SMCT_PAIR_HI_SHIFT	16

#define ZX279133_ISU_CFG		0x10000
#define ZX279133_ISU_INIT_REQ		0x10004
#define ZX279133_ISU_DWRR_WEIGHT	0x10040
#define ZX279133_ISU_RING_GAP		0x1010c
#define ZX279133_ODMA_ISU_SP_EN	0x14000
#define ZX279133_ISU_CFG_MASK		GENMASK(1, 0)
#define ZX279133_ISU_SPA_SP_EN		BIT(1)
#define ZX279133_ISU_INIT_REQ_MASK	GENMASK(2, 0)
#define ZX279133_ISU_INIT_REQ_VALUE	0x7
#define ZX279133_ISU_DWRR_WEIGHT_VALUE	0x00010202
#define ZX279133_ISU_RING_GAP_MASK	GENMASK(11, 0)
#define ZX279133_ISU_RING_GAP_VALUE	3
#define ZX279133_ODMA_ISU_SP_EN_BIT	BIT(0)
#define ZX279133_SPA_IPV6_CRC_MODE	0x8000
#define ZX279133_SPA_IPV6_CRC_BIT	BIT(31)
#define ZX279133_SPA_TCP_CTRL_MASK	GENMASK(19, 12)
#define ZX279133_SPA_TCP_CTRL_VALUE	0x7
#define ZX279133_SPA_TPID_BASE		0x8284
#define ZX279133_SPA_ONU_MAC_HI		0x8300
#define ZX279133_SPA_ONU_MAC_LO		0x8304
#define ZX279133_SPA_TRAP_ETH_TYPE	0x82e4
#define ZX279133_SPA_TRAP_DMAC_HI	0x8398
#define ZX279133_SPA_TRAP_DMAC_LO	0x839c
#define ZX279133_SPA_ONU_STRIDE		0x8
#define ZX279133_SPA_TRAP_DMAC_STRIDE	0x8
#define ZX279133_SPA_MAC_HI_VALUE	0xffffffff
#define ZX279133_SPA_MAC_LO_VALUE	0x7fff
#define ZX279133_SPA_MAC_LO_MASK	GENMASK(15, 0)
#define ZX279133_SPA_ONU_COUNT		16
#define ZX279133_SPA_TRAP_DMAC_COUNT	4
#define ZX279133_WANID_COUNT		32
#define ZX279133_WANID_CPU_MAC_COUNT	2
#define ZX279133_WANID_L3_MTU		1996
#define ZX279133_GLB_OAM_EN		0x10
#define ZX279133_GLB_OAM_MASK		GENMASK(19, 14)
#define ZX279133_GLB_OAM_VALUE		0xf0000

#define ZX279133_SE_PARSER_RAM_DONE	0x40008
#define ZX279133_SE_PARSER_DEBUG_CFG	0x400c0
#define ZX279133_SE_PARSER_G988_CFG	0x400cc
#define ZX279133_SE_SMMU0_RAM_DONE	0x48084
#define ZX279133_SE_STAT_RUNTIME_CFG	0x58520
#define ZX279133_SE_SMMU1_HASH_BASE	0x44000
#define ZX279133_SE_ALG_HASH_CRC	0x500c8
#define ZX279133_SE_ALG_HASH_BULK	0x50208
#define ZX279133_SE_ALG_HASH_DEPTH0	0x50280
#define ZX279133_SE_ALG_HASH_DEPTH1	0x50284
#define ZX279133_SE_PARSER_RAM_MASK	GENMASK(4, 0)
#define ZX279133_SE_PARSER_DEBUG_MASK	GENMASK(1, 0)
#define ZX279133_SE_SMMU0_RAM_MASK	BIT(0)
#define ZX279133_SE_STAT_RUNTIME_MASK	GENMASK(7, 0)

#define ZX279133_PPU_AGCLK_CFG		0x1c
#define ZX279133_PPU_CORE_AGCLK		BIT(3)
#define ZX279133_PPU_REORDER_AGCLK	BIT(12)
#define ZX279133_PPU_CLUSTER_PC		0x90d00
#define ZX279133_PPU_CLUSTER_RDY	0x90d04
#define ZX279133_PPU_CLUSTER_INST_LO	0x90d0c
#define ZX279133_PPU_CLUSTER_INST_HI	0x90d10
#define ZX279133_PPU_CLUSTER_ME_TABLE	0x90350
#define ZX279133_PPU_CLUSTER_ME_WORDS	16
#define ZX279133_PPU_CLUSTER_PKT_TYPE_DETAIL	0x9045c
#define ZX279133_PPU_CLUSTER_PC_MASK	GENMASK(12, 0)
#define ZX279133_PPU_CLUSTER_HI_MASK	GENMASK(26, 0)
#define ZX279133_PPU_IKEY_AFULL		0x9043c
#define ZX279133_PPU_IKEY_AFULL_MASK	GENMASK(4, 0)
#define ZX279133_PPU_BLOCK_RDY		0x800d0
#define ZX279133_PPU_BLOCK_INDEX	0x800d4
#define ZX279133_PPU_BLOCK_DATA	0x800d8
#define ZX279133_PPU_BLOCK_INDEX_MASK	GENMASK(6, 0)
#define ZX279133_PPU_BLOCK_DATA_MASK	GENMASK(23, 0)
#define ZX279133_SPA_IND_CMD		0x8054
#define ZX279133_SPA_IND_STATUS	0x8058
#define ZX279133_SPA_IND_DATA		0x805c
#define ZX279133_SMMU0_CMD		0x48004
#define ZX279133_SMMU0_ADDR		0x48008
#define ZX279133_SMMU0_WDATA		0x4800c
#define ZX279133_SMMU0_RAM_DONE	0x4801c
#define ZX279133_SMMU0_RDATA		0x48024
#define ZX279133_SMMU0_CMD_WRITE	0x8c000000
#define ZX279133_SMMU0_CMD_READ	0x0c000000
#define ZX279133_SMMU0_CMD_MASK	GENMASK(31, 11)
#define ZX279133_SMMU0_ADDR_MASK	GENMASK(27, 0)
#define ZX279133_SMMU0_RAM_MASK	GENMASK(1, 0)
#define ZX279133_SMMU0_WORDS		4
#define ZX279133_FAST_STAT_DEPTH	1024
#define ZX279133_FAST_AGE_DEPTH	4096
#define ZX279133_AGCLK_VALUE_MASK	GENMASK(12, 0)
#define ZX279133_SPA_AUTO_GATE		0xb8
#define ZX279133_SPA_AUTO_GATE_BIT	BIT(12)
#define ZX279133_SPA_IND_CMD_MASK	GENMASK(27, 0)
#define ZX279133_SPA_IND_READ		BIT(27)
#define ZX279133_MCODE_MAGIC		0xffffaaaa
#define ZX279133_MCODE_INST_NUM	12288
#define ZX279133_MCODE_TAG_PKT_TYPE	0xbdbdbdbd
#define ZX279133_MCODE_TAG_DUP		0xcccccccc
#define ZX279133_MCODE_TAG_SKIP		0xdddddddd
#define ZX279133_MCODE_TAG_VERSION	0xaaaaaaaa
#define ZX279133_MCODE_TAG_PORT_FLOW	0xbbbbbbbb

#define ZX279133_IDM_BASE		0x280000
#define ZX279133_IDM_TX_BASE		0x0004
#define ZX279133_IDM_TX_DOORBELL_COUNT_SHIFT	17
#define ZX279133_IDM_TX_QUEUES		4
#define ZX279133_IDM_TX_DEPTH		1024

/*
 * IDM exposes independent RX and CPU-TX descriptor base registers. Both
 * descriptor regions and the active normal RX free ring use DMA-coherent
 * allocations owned by the DMA API. Unused hardware free rings retain the
 * vendor reserved-memory layout.
 */
#define ZX279133_IDM_TX_WINDOW_STRIDE	0x00000800
#define ZX279133_IDM_TX_PAYLOAD_STRIDE	ZX279133_IDM_TX_WINDOW_STRIDE

#define ZX279133_IDM_INT_MASK		0x0040
#define ZX279133_IDM_INT_CPU		0x0044
#define ZX279133_IDM_RX_RELEASE		0x0088
#define ZX279133_IDM_BP_REFILL		0x0100
#define ZX279133_IDM_LOCAL_MASK		BIT(9)
#define ZX279133_IDM_DIRECT_RX_MASK	(GENMASK(15, 0) & \
					 ~ZX279133_IDM_LOCAL_MASK)
#define ZX279133_IDM_NAPI_MASK		(ZX279133_IDM_DIRECT_RX_MASK | \
					 ZX279133_IDM_LOCAL_MASK)
#define ZX279133_IDM_RX_QUEUE		0
#define ZX279133_IDM_RX_QUEUES		24
#define ZX279133_IDM_RX_RING_SIZE	2048
#define ZX279133_IDM_BP_RING_SIZE	4096
#define ZX279133_IDM_RX_BUFFER_COUNT	2048
#define ZX279133_IDM_RX_PAGE_MAP_BITS	12
#define ZX279133_IDM_RX_PAGE_MAP_SIZE	BIT(ZX279133_IDM_RX_PAGE_MAP_BITS)
#define ZX279133_IDM_RX_BUFFER_STRIDE	0x940
#define ZX279133_IDM_RX_BUFFER_SIZE	(ZX279133_IDM_RX_BUFFER_COUNT * \
					 ZX279133_IDM_RX_BUFFER_STRIDE)
#define ZX279133_IDM_RX_PAYLOAD_OFFSET	0x40
#define ZX279133_IDM_RX_FRAME_LIMIT	(ETH_FRAME_LEN + 4)
#define ZX279133_RX_PAGE_ORDER		0
#define ZX279133_RX_PAGE_SIZE		(PAGE_SIZE << ZX279133_RX_PAGE_ORDER)
#define ZX279133_MAX_MTU		1970
#define ZX279133_RX_REFILL_RECOVERY_BATCH	64
#define ZX279133_RX_REFILL_RETRY_MS	100

/*
 * Vendor idm_init() free-ring layout after the legacy 0x1a0000-byte
 * descriptor region: 0x4000-byte RX normal and jumbo rings, followed by
 * 0x40000-byte TX normal, TX jumbo, and CPU133 extra retrieval rings.
 */
#define ZX279133_IDM_FREE_RING_SIZE	0x4000
#define ZX279133_IDM_TX_FREE_RING_SIZE	0x40000
#define ZX279133_IDM_FREE_RING0	0x1a0000
#define ZX279133_IDM_FREE_RING1	(ZX279133_IDM_FREE_RING0 + 0x4000)
#define ZX279133_IDM_FREE_RING2	(ZX279133_IDM_FREE_RING1 + 0x4000)
#define ZX279133_IDM_FREE_RING3	(ZX279133_IDM_FREE_RING2 + 0x40000)
#define ZX279133_IDM_FREE_RING4	(ZX279133_IDM_FREE_RING3 + 0x40000)
#define ZX279133_IDM_REQUIRED_SIZE	(ZX279133_IDM_FREE_RING4 + \
					 ZX279133_IDM_TX_FREE_RING_SIZE)
#define ZX279133_IDM_TX_CONTROL		0x00400000

/*
 * Vendor kernel globals (kernel-2b5.elf rodata/BSS):
 * uIDM_RX_QUEUE_DESC_DEPTH = 2048, uIDM_RX_CFG_DEPTH = 1,
 * uNPPT_IDM_DESC_MODE = 0, uIDM_TX_CFG_DEPTH = 0 (no writer found).
 */
#define ZX279133_IDM_RX_QUEUE_DESC_DEPTH 2048
#define ZX279133_IDM_RX_CFG_DEPTH	1
#define ZX279133_IDM_DESC_MODE		0
#define ZX279133_IDM_TX_CFG_DEPTH	0

/*
 * Per-queue doorbells and completion counters, recovered from
 * idm_cpu_nb_tx_update() and idm_get_tx_done(): queue 0 uses 0x080/0x084,
 * queue n > 0 uses 4 * (n + 39) / 4 * (n + 42).
 */
static inline u32 zx279133_idm_tx_doorbell_reg(unsigned int queue)
{
	return queue == 0 ? 0x0080 : 4 * (queue + 39);
}

static inline u32 zx279133_idm_tx_done_reg(unsigned int queue)
{
	return queue == 0 ? 0x0084 : 4 * (queue + 42);
}

#define ZX279133_XMAC1_BASE	0x180000
#define ZX279133_XMAC_TX_CTRL	0x0000
#define ZX279133_XMAC_RX_CTRL	0x0010
#define ZX279133_XMAC_FRAME_CFG	0x0020
#define ZX279133_XMAC_MODE_CFG	0x0280
#define ZX279133_XMAC_DUPLEX	0x0500
#define ZX279133_XMAC_MISC_CFG	0x3400
#define ZX279133_XMAC_ENABLE	BIT(0)
#define ZX279133_XMAC_HALF_DUPLEX BIT(24)
#define ZX279133_XMAC_SPEED_MASK	GENMASK(31, 29)
#define ZX279133_XMAC_FLOW_CTRL	0x01c0
#define ZX279133_XMAC_TFE		BIT(1)
#define ZX279133_XMAC_PT_MASK		GENMASK(31, 16)
#define ZX279133_XMAC_PAUSE_TIME	0xffff
#define ZX279133_XMAC_RX_FLOW		0x0240
#define ZX279133_XMAC_RX_FLOW_EN	BIT(0)
#define ZX279133_XMAC_RESET_MASK	BIT(11)
#define ZX279133_XMAC_STAT_TX_FRAMES	0x2070
#define ZX279133_XMAC_STAT_TX_GOOD	0x20b0
#define ZX279133_NPPT_XMAC_ERR		0x00fc
#define ZX279133_SMCT_DONE		0xc0f0
#define ZX279133_SSCH5			0x2c054
#define ZX279133_SOPC_RR5		0x340bc
#define ZX279133_SOPC_TO_SMAC5		0x34108
#define ZX279133_SOPC_REQ_SMAC5		0x34134
#define ZX279133_XMAC1_SOPC_READY	0x342a8
#define ZX279133_XMAC1_SOPC_SEND_ENABLE	0x342c4
#define ZX279133_XMAC1_SOPC_DUPLEX_MASK	BIT(5)
#define ZX279133_XMAC1_SGMII_SPEED	3

#define ZX279133_XPCS_BYPASS_REG	0x8005
#define ZX279133_XPCS_BYPASS_EN	BIT(4)

struct zx279133_idm_desc {
	__le32 address;
	__le32 length_flags;
	__le32 metadata[6];
};

struct zx279133_tx_slot {
	struct sk_buff *skb;
	struct net_device *ndev;
	void *bounce;
	dma_addr_t dma;
	u32 len;
	bool dma_mapped;
};

struct zx279133_rx_page_entry {
	struct page *page;
	u32 key;
};

static_assert(sizeof(struct zx279133_idm_desc) == 32);
static_assert(ZX279133_BMU_BPPE_SIZE +
	      ZX279133_BMU_NORMAL_COUNT * ZX279133_BMU_NORMAL_SIZE +
	      ZX279133_BMU_JUMBO_COUNT * ZX279133_BMU_JUMBO_SIZE +
	      ZX279133_BMU_DESC_SIZE == ZX279133_BMU_REQUIRED_SIZE);

struct zx279133_eth;
struct zx279133_flow_offload;

struct zx279133_eth {
	struct device *dev;
	struct net_device *ndev;
	struct net_device *lan_ndev;
	struct zx279133_flow_offload *flow_offload;
	struct zx279133_lan_service lan_service;
	struct zx279133_netdev_stats stats;
	void __iomem *base;
	void __iomem *pps_base;
	struct regmap *pon_route;
	struct regmap *idm_cci;
	phys_addr_t bmu_base;
	phys_addr_t bmu_size;
	void __iomem *bmu_mem;
	phys_addr_t se_hash_base;
	phys_addr_t se_hash_size;
	void __iomem *se_hash_mem;
	phys_addr_t idm_base;
	phys_addr_t idm_size;
	void __iomem *idm_mem;
	struct page_pool *rx_page_pool;
	struct zx279133_rx_page_entry *rx_page_map;
	u16 rx_page_map_count;
	u32 idm_cci_saved[2];
	u32 sipc_saved[2];
	u32 idm_cfg_saved[41];
	u32 greg_saved[10];
	u32 dma_saved[5];
	u32 bmu_saved[18];
	u32 qmg_saved[7];
	u32 sopc_saved[2];
	u32 red_saved[13];
	u32 red_np1_saved[14];
	u32 ssch_saved[4];
	u32 wsch_saved[2];
	u32 woe_sch_saved[2];
	u32 usch_saved[2];
	u32 uopc_saved[14];
	u32 se_parser_debug_saved;
	u32 se_hash_smmu1_saved[8];
	u32 se_hash_alg_crc_saved;
	u32 se_hash_alg_bulk_saved;
	u32 se_hash_alg_depth_saved[2];
	u32 se_hash_id_map[4];
	u32 se_multi_idx_map;
	u32 smct_saved[18];
	u32 sdet_saved[2];
	u32 nppu_early_saved[5];
	u32 nppu_late_saved[2];
	u32 ppu_cluster_me_saved[ZX279133_PPU_CLUSTER_ME_WORDS];
	u32 ppu_cluster_pkt_type_saved;
	u32 spa_saved[47];
	struct clk_bulk_data clocks[ZX279133_NUM_CLOCKS];
	struct phy *serdes;
	struct dw_xpcs *xpcs;
	struct mdio_device *xpcs_mdiodev;
	struct phylink_config phylink_config;
	struct phylink *phylink;
	phy_interface_t host_interface;
	phy_interface_t serdes_interface;
	struct zx279133_idm_desc *rx_descs;
	struct zx279133_idm_desc *tx_descs;
	__be32 *rx_normal_bp;
	dma_addr_t rx_descs_dma;
	dma_addr_t rx_normal_bp_dma;
	dma_addr_t tx_descs_dma;
	u16 rx_cons[ZX279133_IDM_RX_QUEUES];
	u16 rx_bp_prod;
	u8 rx_poll_cursor;
	struct zx279133_tx_slot *tx_slots;
	void *tx_payload;
	dma_addr_t tx_payload_dma;
	u32 idm_tx_base_saved;
	u16 tx_done;
	u16 tx_producer;
	u16 tx_consumer;
	u16 tx_pending;
	u16 tx_notify_pending;
	u64 tx_doorbell_writes;
	u64 tx_doorbell_descs;
	u64 tx_reclaim_polls;
	u64 tx_reclaim_work_runs;
	u64 tx_reclaim_work_packets;
	u64 tx_timeouts;
	u64 tx_timeout_recoveries;
	u64 tx_timeout_stalls;
	u64 tx_hw_csum_packets;
	u64 tx_sw_csum_packets;
	atomic64_t rx_irq_count;
	atomic64_t idm_local_irq_count;
	struct u64_stats_sync rx_stats_sync;
	u64_stats_t rx_napi_polls;
	u64_stats_t rx_napi_work;
	u64_stats_t rx_napi_budget_exhaustions;
	u64_stats_t rx_desc_not_ready;
	u64_stats_t rx_invalid_dma;
	u64_stats_t rx_page_lookup_misses;
	u64_stats_t rx_jumbo_drops;
	u64_stats_t rx_descriptor_flag_drops;
	u64_stats_t rx_page_alloc_failures;
	u64_stats_t rx_copy_fallbacks;
	u64_stats_t rx_skb_alloc_failures;
	u64_stats_t rx_refill_post_failures;
	u64_stats_t rx_refill_shortfalls;
	u64_stats_t rx_refill_published;
	u64_stats_t rx_release_published;
	u64_stats_t rx_refill_recovery_attempts;
	u64_stats_t rx_refill_recovery_pages;
	u64_stats_t rx_refill_recovery_failures;
	atomic64_t rx_refill_retry_work_runs;
	u16 rx_refill_deficit;
	u16 rx_refill_deficit_high_water;
	u16 rx_page_map_high_water;
	/* Serializes TX ring ownership against NAPI completion. */
	spinlock_t tx_lock;
	/* Serializes RX and local interrupt enable state. */
	spinlock_t irq_lock;
	/* Serializes shared XMAC reset and SOPC state with the LAN child. */
	struct mutex xmac_lock;
	/* Serializes ownership of the shared NPPT/IDM datapath. */
	struct mutex datapath_lock;
	struct napi_struct napi;
	struct delayed_work tx_reclaim_work;
	struct delayed_work rx_refill_work;
	unsigned long datapath_users;
	bool hardware_prepared;
	bool lan_datapath_ready;
	bool lan_vlan62_active;
	bool lan_dsa_active;
	bool serdes_powered;
	bool np_reset_prepared;
	bool greg_prepared;
	bool dma_prepared;
	bool bmu_prepared;
	bool qmg_prepared;
	bool sopc_prepared;
	bool red_prepared;
	bool ssch_prepared;
	bool wsch_prepared;
	bool woe_sch_prepared;
	bool usch_prepared;
	bool uopc_prepared;
	bool se_frontend_prepared;
	bool se_hash_prepared;
	bool se_age_done;
	bool ppu_mcode_prepared;
	bool spa_prepared;
	bool nppu_early_prepared;
	bool smct_prepared;
	bool nppu_late_prepared;
	bool ppu_cluster_runtime_prepared;
	bool sdet_prepared;
	bool sipc_final_prepared;
	bool tx_prepared;
	bool rx_prepared;
	bool rx_running;
	bool rx_irq_enabled;
	bool idm_local_irq_enabled;
	bool napi_enabled;
	bool xpcs_runtime_held;
	bool tx_stopping;
	int irqs[ZX279133_NUM_IRQS];
};

static inline struct zx279133_netdev_stats *
zx279133_netdev_stats(struct zx279133_eth *eth, struct net_device *ndev)
{
	if (ndev == eth->ndev)
		return &eth->stats;

	return &((struct zx279133_lan_netdev_priv *)netdev_priv(ndev))->stats;
}

static inline void
zx279133_stats_rx_error(struct zx279133_eth *eth, struct net_device *ndev)
{
	atomic64_inc(&zx279133_netdev_stats(eth, ndev)->rx_errors);
}

static inline void
zx279133_stats_tx_error(struct zx279133_eth *eth, struct net_device *ndev)
{
	atomic64_inc(&zx279133_netdev_stats(eth, ndev)->tx_errors);
}

static inline void
zx279133_stats_rx_dropped(struct zx279133_eth *eth, struct net_device *ndev)
{
	atomic64_inc(&zx279133_netdev_stats(eth, ndev)->rx_dropped);
}

static inline void
zx279133_stats_tx_dropped(struct zx279133_eth *eth, struct net_device *ndev)
{
	atomic64_inc(&zx279133_netdev_stats(eth, ndev)->tx_dropped);
}

static inline void
zx279133_stats_rx_length_error(struct zx279133_eth *eth, struct net_device *ndev)
{
	atomic64_inc(&zx279133_netdev_stats(eth, ndev)->rx_length_errors);
}

int zx279133_tm_prepare(struct zx279133_eth *eth);
void zx279133_tm_restore(struct zx279133_eth *eth);
int zx279133_np_prepare(struct zx279133_eth *eth);
void zx279133_np_restore(struct zx279133_eth *eth);
void zx279133_route_set(struct zx279133_eth *eth, bool enabled);
int zx279133_vlan_runtime_prepare(struct zx279133_eth *eth);
int zx279133_wan_port_bringup(struct zx279133_eth *eth);
void zx279133_program_spa_cpu_mac(struct zx279133_eth *eth,
				  const unsigned char *addr);
int zx279133_program_wanid_cpu_mac(struct zx279133_eth *eth,
				   const unsigned char *addr);
int zx279133_program_wanid_sip(struct zx279133_eth *eth, u32 wanid,
			       u32 sip, u32 *old_sip);
int zx279133_program_wanid_pppoe(struct zx279133_eth *eth, u32 wanid,
				 u8 mode, u16 sid, u8 *old_mode,
				 u16 *old_sid);
int zx279133_fast_ikey_write(struct zx279133_eth *eth, u32 index,
			     const u32 *data);
int zx279133_fast_stats_read(struct zx279133_eth *eth, u16 flow_id,
			     u64 *packets, u64 *bytes);
int zx279133_fast_age_read_clear(struct zx279133_eth *eth, u16 age,
				 bool *used);
int zx279133_flow_offload_init(struct zx279133_eth *eth);
void zx279133_flow_offload_flush(struct zx279133_eth *eth);
int zx279133_flow_offload_setup_tc(struct zx279133_eth *eth,
				   struct net_device *ndev,
				   enum tc_setup_type type, void *type_data);

void zx279133_idm_set_masked(struct zx279133_eth *eth, u32 mask, bool masked);
unsigned int zx279133_idm_tx_reclaim_locked(struct zx279133_eth *eth);
void zx279133_idm_rx_refill_work(struct work_struct *work);
int zx279133_idm_rx_poll(struct napi_struct *napi, int budget);
irqreturn_t zx279133_idm_rx_irq(int irq, void *data);
irqreturn_t zx279133_idm_local_irq(int irq, void *data);
int zx279133_idm_rx_prepare(struct zx279133_eth *eth);
void zx279133_idm_rx_release(struct zx279133_eth *eth);
void zx279133_idm_tx_flush_locked(struct zx279133_eth *eth);
void zx279133_idm_tx_reclaim_work(struct work_struct *work);
bool zx279133_idm_tx_drain(struct zx279133_eth *eth);
int zx279133_idm_tx_prepare(struct zx279133_eth *eth);
void zx279133_idm_tx_deactivate(struct zx279133_eth *eth);
void zx279133_idm_tx_release(struct zx279133_eth *eth, bool hardware_alive);

void zx279133_xmac_set_enabled(struct zx279133_eth *eth, bool enabled);
int zx279133_xpcs_set_bypass(struct zx279133_eth *eth, bool enabled);
extern const struct phylink_mac_ops zx279133_phylink_ops;

int zx279133_hardware_prepare(struct zx279133_eth *eth);
void zx279133_hardware_unprepare(struct zx279133_eth *eth);
extern const struct ethtool_ops zx279133_ethtool_ops;
extern const struct net_device_ops zx279133_netdev_ops;
extern const struct zx279133_lan_service_ops zx279133_lan_service_ops;

#endif /* __ZX279133_H__ */
