// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/regmap.h>
#include <linux/unaligned.h>

#include "zx279133.h"

void zx279133_route_set(struct zx279133_eth *eth, bool enabled)
{
	u32 value;

	value = readl(eth->base + ZX279133_ROUTE_CLK);
	writel(enabled ? value | ZX279133_ROUTE_CLK_EN :
			 value & ~ZX279133_ROUTE_CLK_EN,
	       eth->base + ZX279133_ROUTE_CLK);

	regmap_write(eth->pon_route, 0, enabled ? 0 : 1);

	value = readl(eth->base + ZX279133_ROUTE_CTRL);
	writel(enabled ? value & ~ZX279133_ROUTE_XMAC1 :
			 value | ZX279133_ROUTE_XMAC1,
	       eth->base + ZX279133_ROUTE_CTRL);
}

static const u32 zx279133_smct_offsets[] = {
	ZX279133_SMCT_SEL_CFG0,
	ZX279133_SMCT_SEL_CFG1,
	ZX279133_SMCT_CTRL,
	ZX279133_SMCT_COS_TH0,
	ZX279133_SMCT_COS_TH0 + 0x4,
	ZX279133_SMCT_COS_TH0 + 0x8,
	ZX279133_SMCT_COS_TH0 + 0xc,
	ZX279133_SMCT_INPORT_TH0,
	ZX279133_SMCT_INPORT_TH0 + 0x4,
	ZX279133_SMCT_INPORT_TH0 + 0x8,
	ZX279133_SMCT_INPORT_TH0 + 0xc,
	ZX279133_SMCT_INPORT_TH0 + 0x10,
	ZX279133_SMCT_INPORT_TH0 + 0x14,
	ZX279133_SMCT_INPORT_TH0 + 0x18,
	ZX279133_SMCT_INPORT_TH0 + 0x1c,
	ZX279133_SMCT_MC_TH,
	ZX279133_SMCT_NP1_TH,
	ZX279133_SMCT_IDM_SSCH_TH,
};

static const u32 zx279133_smct_cos_pairs[] = {
	ZX279133_SMCT_COS_TH_VALUE |
		ZX279133_SMCT_COS_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_COS_TH_VALUE |
		ZX279133_SMCT_COS_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_COS_TH_VALUE |
		ZX279133_SMCT_COS_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_COS_LAST_VALUE |
		ZX279133_SMCT_COS7_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
};

static const u32 zx279133_smct_inport_pairs[] = {
	ZX279133_SMCT_INPORT_TH_VALUE |
		ZX279133_SMCT_INPORT_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_INPORT_TH_VALUE |
		ZX279133_SMCT_INPORT_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_INPORT_TH_VALUE |
		ZX279133_SMCT_INPORT_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_INPORT_TH_VALUE |
		ZX279133_SMCT_INPORT_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_COS7_TH_VALUE |
		ZX279133_SMCT_INPORT_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_INPORT_TH_VALUE |
		ZX279133_SMCT_COS7_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_COS_LAST_VALUE |
		ZX279133_SMCT_COS_LAST_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	ZX279133_SMCT_COS_LAST_VALUE |
		ZX279133_SMCT_INPORT_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
};

static void zx279133_smct_prepare(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(zx279133_smct_offsets); i++)
		eth->smct_saved[i] = readl(eth->base + zx279133_smct_offsets[i]);

	writel(ZX279133_SMCT_SEL_CFG0_VALUE,
	       eth->base + ZX279133_SMCT_SEL_CFG0);
	writel(ZX279133_SMCT_SEL_CFG1_VALUE,
	       eth->base + ZX279133_SMCT_SEL_CFG1);
	writel(ZX279133_SMCT_CTRL_VALUE, eth->base + ZX279133_SMCT_CTRL);
	for (i = 0; i < ARRAY_SIZE(zx279133_smct_cos_pairs); i++)
		writel((eth->smct_saved[3 + i] &
			ZX279133_SMCT_PAIR_PRESERVE_MASK) |
		       zx279133_smct_cos_pairs[i],
		       eth->base + ZX279133_SMCT_COS_TH0 + 4 * i);
	for (i = 0; i < ARRAY_SIZE(zx279133_smct_inport_pairs); i++)
		writel((eth->smct_saved[7 + i] &
			ZX279133_SMCT_PAIR_PRESERVE_MASK) |
		       zx279133_smct_inport_pairs[i],
		       eth->base + ZX279133_SMCT_INPORT_TH0 + 4 * i);
	writel((eth->smct_saved[15] & ~ZX279133_SMCT_MC_TH_MASK) |
	       ZX279133_SMCT_MC_TH_VALUE,
	       eth->base + ZX279133_SMCT_MC_TH);
	writel(ZX279133_SMCT_NP1_TH_VALUE, eth->base + ZX279133_SMCT_NP1_TH);
	writel((eth->smct_saved[17] & GENMASK(31, 30)) |
	       ZX279133_SMCT_NP1_TH_VALUE |
	       ZX279133_SMCT_NP1_TH_VALUE << ZX279133_SMCT_PAIR_HI_SHIFT,
	       eth->base + ZX279133_SMCT_IDM_SSCH_TH);
	eth->smct_prepared = true;
}

static void zx279133_smct_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->smct_prepared)
		return;

	for (i = 0; i < ARRAY_SIZE(zx279133_smct_offsets); i++)
		writel(eth->smct_saved[i], eth->base + zx279133_smct_offsets[i]);
	eth->smct_prepared = false;
}

static int
zx279133_ppu_inst_block(struct zx279133_eth *eth, u32 pc, const u32 *lo, const u32 *hi)
{
	u32 value;
	int ret;
	int i;

	ret = readl_poll_timeout_atomic(eth->pps_base +
					ZX279133_PPU_CLUSTER_RDY,
					value, value & BIT(0), 1, 1000);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "PPU cluster not ready: %#x\n", value);
	for (i = 0; i < 4; i++) {
		writel(lo[i],
		       eth->pps_base + ZX279133_PPU_CLUSTER_INST_LO + 8 * i);
		writel((readl(eth->pps_base + ZX279133_PPU_CLUSTER_INST_HI +
			      8 * i) & ~ZX279133_PPU_CLUSTER_HI_MASK) | hi[i],
		       eth->pps_base + ZX279133_PPU_CLUSTER_INST_HI + 8 * i);
	}
	writel((readl(eth->pps_base + ZX279133_PPU_CLUSTER_PC) &
		~ZX279133_PPU_CLUSTER_PC_MASK) | pc >> 2,
	       eth->pps_base + ZX279133_PPU_CLUSTER_PC);
	ret = readl_poll_timeout_atomic(eth->pps_base +
					ZX279133_PPU_CLUSTER_PC,
					value, value & BIT(0), 1, 1000);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "PPU instruction block timeout: %#x\n",
				     value);

	return 0;
}

/*
 * CPU133's factory PPU image assigns every fourth flow to ME0.  The reset
 * value leaves the cluster with no executable ME for the CPU-TX flow, which
 * is reported later as a DMA drop.  This is the runtime table used by
 * ppu_cluster_set_me_val_by_flow_num(), not PPU microcode.
 */
static void zx279133_ppu_cluster_runtime_prepare(struct zx279133_eth *eth)
{
	unsigned int i;

	for (i = 0; i < ZX279133_PPU_CLUSTER_ME_WORDS; i++)
		eth->ppu_cluster_me_saved[i] =
			readl(eth->pps_base + ZX279133_PPU_CLUSTER_ME_TABLE + 4 * i);
	eth->ppu_cluster_pkt_type_saved =
		readl(eth->pps_base + ZX279133_PPU_CLUSTER_PKT_TYPE_DETAIL);

	for (i = 0; i < ZX279133_PPU_CLUSTER_ME_WORDS; i++)
		writel(0x00000001,
		       eth->pps_base + ZX279133_PPU_CLUSTER_ME_TABLE + 4 * i);
	writel(1, eth->pps_base + ZX279133_PPU_CLUSTER_PKT_TYPE_DETAIL);
	eth->ppu_cluster_runtime_prepared = true;
}

static void zx279133_ppu_cluster_runtime_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->ppu_cluster_runtime_prepared)
		return;

	for (i = 0; i < ZX279133_PPU_CLUSTER_ME_WORDS; i++)
		writel(eth->ppu_cluster_me_saved[i],
		       eth->pps_base + ZX279133_PPU_CLUSTER_ME_TABLE + 4 * i);
	writel(eth->ppu_cluster_pkt_type_saved,
	       eth->pps_base + ZX279133_PPU_CLUSTER_PKT_TYPE_DETAIL);
	eth->ppu_cluster_runtime_prepared = false;
}

static int zx279133_spa_indirect_write(struct zx279133_eth *eth, u32 mem_id,
				       u32 addr, u32 value)
{
	u32 gate, status;
	int ret;

	gate = readl(eth->base + ZX279133_SPA_AUTO_GATE);
	if (gate & ZX279133_SPA_AUTO_GATE_BIT)
		writel(gate & ~ZX279133_SPA_AUTO_GATE_BIT,
		       eth->base + ZX279133_SPA_AUTO_GATE);

	ret = readl_poll_timeout_atomic(eth->base + ZX279133_SPA_IND_STATUS,
					status, status & BIT(0), 1, 1000);
	if (ret) {
		dev_err_probe(eth->dev, ret,
			      "SPA indirect access not done: %#x\n", status);
		goto out;
	}
	writel((readl(eth->base + ZX279133_SPA_IND_CMD) &
		~ZX279133_SPA_IND_CMD_MASK) |
	       mem_id << 20 | addr,
	       eth->base + ZX279133_SPA_IND_CMD);
	writel(value, eth->base + ZX279133_SPA_IND_DATA);

out:
	if (gate & ZX279133_SPA_AUTO_GATE_BIT)
		writel(gate, eth->base + ZX279133_SPA_AUTO_GATE);
	return ret;
}

static int zx279133_spa_indirect_read(struct zx279133_eth *eth, u32 mem_id,
				      u32 addr, u32 *value)
{
	u32 gate, status;
	int ret;

	gate = readl(eth->base + ZX279133_SPA_AUTO_GATE);
	if (gate & ZX279133_SPA_AUTO_GATE_BIT)
		writel(gate & ~ZX279133_SPA_AUTO_GATE_BIT,
		       eth->base + ZX279133_SPA_AUTO_GATE);

	ret = readl_poll_timeout_atomic(eth->base + ZX279133_SPA_IND_STATUS,
					status, status & BIT(0), 1, 1000);
	if (ret) {
		dev_err_probe(eth->dev, ret,
			      "SPA indirect access not done: %#x\n", status);
		goto out;
	}
	writel((readl(eth->base + ZX279133_SPA_IND_CMD) &
		~ZX279133_SPA_IND_CMD_MASK) |
	       ZX279133_SPA_IND_READ | mem_id << 20 | addr,
	       eth->base + ZX279133_SPA_IND_CMD);
	ret = readl_poll_timeout_atomic(eth->base + ZX279133_SPA_IND_STATUS,
					status, status & BIT(0), 1, 1000);
	if (ret) {
		dev_err_probe(eth->dev, ret,
			      "SPA indirect read not done: %#x\n", status);
		goto out;
	}
	*value = readl(eth->base + ZX279133_SPA_IND_DATA);

out:
	if (gate & ZX279133_SPA_AUTO_GATE_BIT)
		writel(gate, eth->base + ZX279133_SPA_AUTO_GATE);
	return ret;
}

/*
 * SE SMMU0 indirect RAM access, recovered from np.ko se_smmu0_read()/
 * se_smmu0_write(): address, four data words and command live in the PPS
 * block; every transfer first polls the RAM-done status, and the SE i-key
 * auto-gate clock is quiesced around the access when enabled, exactly
 * like the vendor driver does.
 */
static int zx279133_smmu0_wait(struct zx279133_eth *eth)
{
	u32 status;

	return readl_poll_timeout_atomic(eth->pps_base +
					 ZX279133_SMMU0_RAM_DONE,
					 status, status & ZX279133_SMMU0_RAM_MASK,
					 1, 1000);
}

static int zx279133_smmu0_read(struct zx279133_eth *eth, u32 addr_ind,
			       u32 *data, u32 words, u32 cmd)
{
	u32 agclk = readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) &
		    ZX279133_AGCLK_VALUE_MASK;
	u32 reg;
	int ret;
	int i;

	if (agclk == 1)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) &
		       ~BIT(0), eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	ret = zx279133_smmu0_wait(eth);
	if (ret)
		goto out;

	reg = readl(eth->pps_base + ZX279133_SMMU0_ADDR);
	writel((addr_ind & ZX279133_SMMU0_ADDR_MASK) |
	       (reg & ~ZX279133_SMMU0_ADDR_MASK),
	       eth->pps_base + ZX279133_SMMU0_ADDR);
	reg = readl(eth->pps_base + ZX279133_SMMU0_CMD);
	writel((cmd & ZX279133_SMMU0_CMD_MASK) |
	       (reg & ~ZX279133_SMMU0_CMD_MASK),
	       eth->pps_base + ZX279133_SMMU0_CMD);
	ret = zx279133_smmu0_wait(eth);
	if (ret)
		goto out;
	for (i = 0; i < ZX279133_SMMU0_WORDS; i++)
		data[i] = readl(eth->pps_base + ZX279133_SMMU0_RDATA +
				4 * (ZX279133_SMMU0_WORDS - 1 - i));

out:
	if (agclk == 1)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) | BIT(0),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	return ret;
}

static int zx279133_smmu0_write(struct zx279133_eth *eth, u32 addr_ind,
				const u32 *data, u32 words, u32 cmd)
{
	u32 agclk = readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) &
		    ZX279133_AGCLK_VALUE_MASK;
	u32 reg;
	int ret;
	int i;

	if (agclk == 1)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) &
		       ~BIT(0), eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	ret = zx279133_smmu0_wait(eth);
	if (ret)
		goto out;

	reg = readl(eth->pps_base + ZX279133_SMMU0_ADDR);
	writel((addr_ind & ZX279133_SMMU0_ADDR_MASK) |
	       (reg & ~ZX279133_SMMU0_ADDR_MASK),
	       eth->pps_base + ZX279133_SMMU0_ADDR);
	for (i = words - 1; i >= 0; i--)
		writel(data[i], eth->pps_base + ZX279133_SMMU0_WDATA + 4 * i);
	reg = readl(eth->pps_base + ZX279133_SMMU0_CMD);
	writel((cmd & ZX279133_SMMU0_CMD_MASK) |
	       (reg & ~ZX279133_SMMU0_CMD_MASK),
	       eth->pps_base + ZX279133_SMMU0_CMD);

out:
	if (agclk == 1)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) | BIT(0),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	return ret;
}

/*
 * SPA port-attribute updates recovered from np.ko, entry 71 (memory
 * address 19), byte 2: mf_port_iswanport_set(0, 1) sets bit 1 on mem_id
 * 0 (companion 14) at np_nppu_init, and mf_port_ppuinit_set() sets bit 2
 * on the mem ids the vendor configures at np_init_done, including CPU
 * injection memory 15 (companion 10 for mem_id 5). Every update is
 * mirrored into the MF extension table, SE ERAM table 50: 64 entries of
 * 128 bits keyed by SPA memory id, word 3
 * carrying the port-attribute word the SPA multi-forward engine consults
 * to accept CPU-originated frames for a port.
 */
#define ZX279133_MF_ERAM_BASE_BLOCK	17106
#define ZX279133_VLAN_ERAM_BASE_BLOCK	12288

static const u32 zx279133_vlan0_entry[ZX279133_SMMU0_WORDS] = {
	0xffc00000, 0x0000ffff, 0xffffffff, 0x00003003,
};

/*
 * np_ppu_init() tail recovered from np.ko: the i-key response FIFO almost
 * full config, the global L2 MTU (SE ERAM sdt 1) and the per-wanid L3 MTU
 * (SE ERAM sdt 2) settings.
 */

void zx279133_program_spa_cpu_mac(struct zx279133_eth *eth, const u8 *addr)
{
	u8 cpu_mac[ETH_ALEN];
	unsigned int i;

	ether_addr_copy(cpu_mac, addr);
	for (i = 0; i < ZX279133_WANID_CPU_MAC_COUNT; i++) {
		writel(get_unaligned_be32(cpu_mac + 2),
		       eth->base + ZX279133_SPA_ONU_MAC_HI +
		       ZX279133_SPA_ONU_STRIDE * i);
		writel((readl(eth->base + ZX279133_SPA_ONU_MAC_LO +
			      ZX279133_SPA_ONU_STRIDE * i) &
			~ZX279133_SPA_MAC_LO_MASK) |
		       get_unaligned_be16(cpu_mac),
		       eth->base + ZX279133_SPA_ONU_MAC_LO +
		       ZX279133_SPA_ONU_STRIDE * i);
		eth_addr_inc(cpu_mac);
	}
}

int zx279133_program_wanid_cpu_mac(struct zx279133_eth *eth, const u8 *addr)
{
	u8 cpu_mac[ETH_ALEN];
	u32 data[4];
	int i, ret;

	ether_addr_copy(cpu_mac, addr);
	for (i = 0; i < ZX279133_WANID_CPU_MAC_COUNT; i++) {
		ret = zx279133_smmu0_read(eth, (16386 + i) << 7, data, 4,
					  ZX279133_SMMU0_CMD_READ);
		if (ret)
			return ret;
		data[2] = get_unaligned_be32(cpu_mac + 2);
		data[3] = (data[3] & ~GENMASK(15, 0)) |
			  get_unaligned_be16(cpu_mac);
		ret = zx279133_smmu0_write(eth, (16386 + i) << 7, data, 4,
					   ZX279133_SMMU0_CMD_WRITE);
		if (ret)
			return ret;
		eth_addr_inc(cpu_mac);
	}
	return 0;
}

int zx279133_program_wanid_sip(struct zx279133_eth *eth, u32 wanid,
			       u32 sip, u32 *old_sip)
{
	u32 data[ZX279133_SMMU0_WORDS];
	int ret;

	if (wanid >= ZX279133_WANID_COUNT)
		return -EINVAL;
	ret = zx279133_smmu0_read(eth, (16386 + wanid) << 7, data,
				  ARRAY_SIZE(data), ZX279133_SMMU0_CMD_READ);
	if (ret)
		return ret;
	if (old_sip)
		*old_sip = data[1];
	data[1] = sip;
	return zx279133_smmu0_write(eth, (16386 + wanid) << 7, data,
				    ARRAY_SIZE(data), ZX279133_SMMU0_CMD_WRITE);
}

#define ZX279133_FAST_IKEY_BASE_BLOCK	23186
#define ZX279133_FAST_STAT_BASE_BLOCK	0x9081
#define ZX279133_FAST_STAT_ENTRY_STRIDE	64
#define ZX279133_SMMU0_CMD_READ64	0x08000000
#define ZX279133_SMMU0_CMD_READ_CLEAR	0x40000000

int zx279133_fast_ikey_write(struct zx279133_eth *eth, u32 index,
			     const u32 *data)
{
	return zx279133_smmu0_write(eth,
				    (ZX279133_FAST_IKEY_BASE_BLOCK + index) << 7,
				    data, ZX279133_SMMU0_WORDS,
				    ZX279133_SMMU0_CMD_WRITE);
}

static int zx279133_fast_stat_counter_read(struct zx279133_eth *eth,
					   u32 index, u64 *counter)
{
	u32 data[ZX279133_SMMU0_WORDS];
	int ret;

	/* A 64-bit SDT read returns the selected entry in words 2 and 3. */
	ret = zx279133_smmu0_read(eth,
				  (ZX279133_FAST_STAT_BASE_BLOCK << 7) +
				  index * ZX279133_FAST_STAT_ENTRY_STRIDE,
				  data, ARRAY_SIZE(data),
				  ZX279133_SMMU0_CMD_READ64);
	if (ret)
		return ret;
	*counter = (u64)data[3] << 32 | data[2];

	return 0;
}

int zx279133_fast_stats_read(struct zx279133_eth *eth, u16 flow_id,
			     u64 *packets, u64 *bytes)
{
	int ret;

	if (flow_id >= ZX279133_FAST_STAT_DEPTH)
		return -EINVAL;
	ret = zx279133_fast_stat_counter_read(eth, 2 * flow_id, packets);
	if (ret)
		return ret;

	return zx279133_fast_stat_counter_read(eth, 2 * flow_id + 1, bytes);
}

int zx279133_fast_age_read_clear(struct zx279133_eth *eth, u16 age,
				 bool *used)
{
	u32 data[ZX279133_SMMU0_WORDS];
	bool first;
	int ret;

	if (age >= ZX279133_FAST_AGE_DEPTH)
		return -EINVAL;
	ret = zx279133_smmu0_read(eth, age, data, ARRAY_SIZE(data),
				  ZX279133_SMMU0_CMD_READ_CLEAR);
	if (ret)
		return ret;
	first = data[3] >> 31;
	ret = zx279133_smmu0_read(eth, age, data, ARRAY_SIZE(data),
				  ZX279133_SMMU0_CMD_READ_CLEAR);
	if (ret)
		return ret;
	*used = first || data[3] >> 31;

	return 0;
}

int zx279133_vlan_runtime_prepare(struct zx279133_eth *eth)
{
	/* Factory VLAN action template for untagged traffic (VID 0). */
	return zx279133_smmu0_write(eth,
				   ZX279133_VLAN_ERAM_BASE_BLOCK << 7,
				   zx279133_vlan0_entry,
				   ARRAY_SIZE(zx279133_vlan0_entry),
				   ZX279133_SMMU0_CMD_WRITE);
}

static int zx279133_np_ppu_init_tail(struct zx279133_eth *eth)
{
	/*
	 * Full-application runtime image from global_table_print() and a
	 * byte-exact factory SMMU0 capture. Besides the 1996-byte L2 MTU it
	 * selects switch UNI5/memory 6 as the external WAN input.
	 */
	/*
	 * se_smmu0_write() maps data[0..3] to WDATA0..3.  RDATA is exposed
	 * in reverse order from WDATA.  global_mf_cnt is the low nibble of
	 * data[0].
	 */
	u32 data[4] = { 0x04500600, 0x01f30036, 0, 0x40904400 };
	u32 reg;
	int ret, i;

	/* ppu_cluster_set_ikey_rsp_fifo_afull_cfg(0): PPS 0x9043c [4:0]. */
	reg = readl(eth->pps_base + 0x9043c);
	writel(reg & ~0x1f, eth->pps_base + 0x9043c);

	/* SE ERAM sdt 1, entry 0, block 16384. */
	ret = zx279133_smmu0_write(eth, 16384 << 7, data, 4,
				   ZX279133_SMMU0_CMD_WRITE);
	if (ret)
		return ret;

	/*
	 * The vendor cspd runtime WANID image uses a 1996-byte L3 limit.
	 * Keep that vendor-compatible CPU-WAN limit; it is not a 9K datapath
	 * configuration. WANIDs 0 and 1 still carry the two CPU MACs.
	 */
	for (i = 0; i < ZX279133_WANID_COUNT; i++) {
		ret = zx279133_smmu0_read(eth, (16386 + i) << 7, data, 4,
					  ZX279133_SMMU0_CMD_READ);
		if (ret)
			return ret;
		data[0] = (data[0] & ~GENMASK(15, 0)) |
			  ZX279133_WANID_L3_MTU;
		ret = zx279133_smmu0_write(eth, (16386 + i) << 7, data, 4,
					   ZX279133_SMMU0_CMD_WRITE);
		if (ret)
			return ret;
	}
	ret = zx279133_program_wanid_cpu_mac(eth, eth->ndev->dev_addr);
	if (ret)
		return ret;

	/* Runtime P2P flow dispatch and packet-type detail used by the ME. */
	zx279133_ppu_cluster_runtime_prepare(eth);

	return 0;
}

static int zx279133_mf_eram_entry_write(struct zx279133_eth *eth, u32 mem_id,
					const u32 *data)
{
	u32 addr_ind = (mem_id + ZX279133_MF_ERAM_BASE_BLOCK) << 7;
	u32 rd[ZX279133_SMMU0_WORDS];
	int ret;

	ret = zx279133_smmu0_write(eth, addr_ind, data,
				   ZX279133_SMMU0_WORDS,
				   ZX279133_SMMU0_CMD_WRITE);
	if (ret)
		return ret;

	/* Preserve the write/readback failure check without production noise. */
	ret = zx279133_smmu0_read(eth, addr_ind, rd, ZX279133_SMMU0_WORDS,
				  ZX279133_SMMU0_CMD_READ);
	if (ret) {
		dev_err(eth->dev, "MF ERAM readback failed: %d\n", ret);
		return ret;
	}
	dev_dbg(eth->dev,
		"MF ERAM mem %u ind 0x%x read %08x %08x %08x %08x\n",
		mem_id, addr_ind, rd[0], rd[1], rd[2], rd[3]);

	return 0;
}

static int zx279133_mf_eram_attr_set(struct zx279133_eth *eth, u32 mem_id,
				     u32 attr)
{
	u32 data[ZX279133_SMMU0_WORDS];
	u32 addr_ind = (mem_id + ZX279133_MF_ERAM_BASE_BLOCK) << 7;
	int ret;

	ret = zx279133_smmu0_read(eth, addr_ind, data, ARRAY_SIZE(data),
				  ZX279133_SMMU0_CMD_READ);
	if (ret)
		return ret;
	data[3] = attr;

	return zx279133_mf_eram_entry_write(eth, mem_id, data);
}

static int zx279133_mf_port_attr_set(struct zx279133_eth *eth, u32 mem_id,
				     u32 companion, u32 bit)
{
	u32 attr;
	int ret;

	ret = zx279133_spa_indirect_read(eth, mem_id, 19, &attr);
	if (ret)
		return ret;
	attr |= bit;
	ret = zx279133_spa_indirect_write(eth, mem_id, 19, attr);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, companion, 19, attr);
	if (ret)
		return ret;
	return zx279133_mf_eram_attr_set(eth, mem_id, attr);
}

static int zx279133_mf_port_ppuinit(struct zx279133_eth *eth)
{
	unsigned int mem_id;
	int ret;

	ret = zx279133_mf_port_attr_set(eth, 0, 14, BIT(17) | BIT(18));
	if (ret)
		return ret;

	for (mem_id = 1; mem_id <= 5; mem_id++) {
		ret = zx279133_mf_port_attr_set(eth, mem_id,
						mem_id == 5 ? 10 : mem_id,
						BIT(18));
		if (ret)
			return ret;
	}

	/* CPU-originated IDM frames enter the PPU through logical port 15. */
	ret = zx279133_mf_port_attr_set(eth, 15, 15, BIT(18));
	if (ret)
		return ret;

	return 0;
}

/*
 * WAN port bring-up recovered from np.ko tm_sw_lan_up_pon_set(): when the
 * switch-side WAN port (5, PPU memory id 6) comes up the vendor driver
 * copies the CPU port's SPA start-PC attribute tables (1/3/4 at memory
 * address 0), flags the port as the WAN port in the SPA attribute table
 * and the MF extension table, installs the WAN port's 128-entry PRO table
 * (SE ERAM table 4, 16-bit entries), and enables the LAN-up port in the
 * NPPT globals. The CPU-originated frames then carry port 6.
 */
static int zx279133_mf_port_startpc_copy(struct zx279133_eth *eth,
					 u32 dst_mem, u32 src_mem)
{
	u32 src, dst;
	int ret;

	ret = zx279133_spa_indirect_read(eth, src_mem, 0, &src);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_read(eth, dst_mem, 0, &dst);
	if (ret)
		return ret;
	dst = (dst & ~GENMASK(23, 0)) | (src & GENMASK(23, 0));

	return zx279133_spa_indirect_write(eth, dst_mem, 0, dst);
}

#define ZX279133_PRO_ERAM_BASE_BLOCK	21266
#define ZX279133_PRO_BLOCKS_PER_PORT	16
#define ZX279133_LAN_INGRESS_PPU_PORT	0
#define ZX279133_LAN_PPU_PORT		5
#define ZX279133_WAN_PPU_PORT		6
#define ZX279133_LAN_COMPANION_PORT	10

static const u32 zx279133_wan_proto_table[ZX279133_PRO_BLOCKS_PER_PORT]
					     [ZX279133_SMMU0_WORDS] = {
	{ 0x7c3c7c3c, 0x30307c3c, 0x70303030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70307030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70307030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70307030, 0x70307434 },
	{ 0x74347030, 0x70307030, 0x70307030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70303030, 0x30303030 },
	{ 0x30307030, 0x70307030, 0x70307030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x78387838, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70307030, 0x30307030 },
	{ 0x2c2c7030, 0x2c2c6022, 0x70302c2c, 0x6c2c6c2c },
	{ 0x70307030, 0x70307030, 0x70306030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70307030, 0x70307030 },
	{ 0x6c2c6c2c, 0x70307030, 0x70307030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70307030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70307030, 0x70307030 },
	{ 0x70307030, 0x70307030, 0x70307030, 0x70307030 },
};

static const u32 zx279133_lan_proto_table[ZX279133_PRO_BLOCKS_PER_PORT]
					     [ZX279133_SMMU0_WORDS] = {
	{ 0x40024002, 0x14144002, 0x58181818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x58185010 },
	{ 0x50105818, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58181818, 0x18181818 },
	{ 0x18185818, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x18185818 },
	{ 0x08085818, 0x48084002, 0x58180808, 0x48084808 },
	{ 0x54145414, 0x58185414, 0x58184014, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x48084808, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x58185818 },
	{ 0x58185818, 0x58185818, 0x58185818, 0x58185818 },
};

static int
zx279133_proto_table_write(struct zx279133_eth *eth, u32 port,
			   const u32 table[][ZX279133_SMMU0_WORDS])
{
	u32 block = ZX279133_PRO_ERAM_BASE_BLOCK +
		    port * ZX279133_PRO_BLOCKS_PER_PORT;
	int i;
	int ret;

	for (i = 0; i < ZX279133_PRO_BLOCKS_PER_PORT; i++) {
		ret = zx279133_smmu0_write(eth, (block + i) << 7,
					   table[i],
					   ZX279133_SMMU0_WORDS,
					   ZX279133_SMMU0_CMD_WRITE);
		if (ret)
			return ret;
	}

	return 0;
}

static int zx279133_spa_pkt_type_runtime(struct zx279133_eth *eth)
{
	static const u16 idx[] = { 0, 1, 128, 129 };
	int i;

	for (i = 0; i < ARRAY_SIZE(idx); i++) {
		int ret = zx279133_spa_indirect_write(eth, 0x40, idx[i],
						      0x00f7c000);

		if (ret)
			return ret;
	}

	return 0;
}

int zx279133_wan_port_bringup(struct zx279133_eth *eth)
{
	u32 reg;
	int ret;

	/*
	 * The vendor runtime (switch.ko port init) programs the CPU port
	 * memory 0 with Tbl Vld + start Pc 561 (SPA address 0 = 0x463,
	 * observed on the factory system); the start-PC copy below then
	 * propagates it to the WAN port. The switch-side UNI 5 maps to PPU
	 * port and SPA memory 6 (getPort(5) = 6). The factory's final
	 * memory-5 LAN state remains address 0 = 0x8015 (Pc 10, flow 1),
	 * address 10 = 0x01F4000C and address 19 = 0x42000.
	 * Forcing start-PC 0 to match vendor ODMA start_pc broke even 56-byte
	 * RX; ODMA start_pc=0 is post-PPU, not the ingress SPA start.
	 */
	ret = zx279133_spa_indirect_read(eth, 0, 0, &reg);
	if (ret)
		return ret;
	reg = (reg & ~GENMASK(23, 0)) | 0x463;
	ret = zx279133_spa_indirect_write(eth, 0, 0, reg);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 0, 10, 0x01f4000c);
	if (ret)
		return ret;

	ret = zx279133_mf_port_startpc_copy(eth, 6, 0);
	if (ret)
		return ret;
	/*
	 * RTL8372N strips the private transport VLAN before XMAC0.  Its real
	 * ingress path is SPA memory 5, so use the ordinary-Ethernet entry PC;
	 * the factory LAN PC expects the four-byte private header to remain.
	 */
	ret = zx279133_spa_indirect_read(eth, 5, 0, &reg);
	if (ret)
		return ret;
	reg = (reg & ~GENMASK(23, 0)) | 0x463;
	ret = zx279133_spa_indirect_write(eth, 5, 0, reg);
	if (ret)
		return ret;
	/*
	 * RTL8372N ingress reaches the PPU through port 0; ports 5 and 10 cover
	 * the CPU-facing and companion LAN paths. Retain the WAN image on port 6.
	 */
	ret = zx279133_proto_table_write(eth, ZX279133_LAN_INGRESS_PPU_PORT,
					 zx279133_lan_proto_table);
	if (ret)
		return ret;
	ret = zx279133_proto_table_write(eth, ZX279133_LAN_PPU_PORT,
					 zx279133_lan_proto_table);
	if (ret)
		return ret;
	ret = zx279133_proto_table_write(eth, ZX279133_WAN_PPU_PORT,
					 zx279133_wan_proto_table);
	if (ret)
		return ret;
	ret = zx279133_proto_table_write(eth, ZX279133_LAN_COMPANION_PORT,
					 zx279133_lan_proto_table);
	if (ret)
		return ret;

	ret = zx279133_spa_indirect_write(eth, 10, 0, 0x00c0800b);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 10, 1, 0x0000088e);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 10, 10, 0x01f4000c);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 10, 13, 0x00008008);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 10, 19, 0x00042000);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 15, 10, 0x01f4000c);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 5, 1, 0x0000088f);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 5, 10, 0x01f4000c);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 5, 13, 0x00650650);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 5, 14, 0x00650650);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 5, 19, 0x00042000);
	if (ret)
		return ret;
	ret = zx279133_mf_eram_attr_set(eth, 5, 0x00042000);
	if (ret)
		return ret;

	/*
	 * PPU port 6's final factory WAN state: MF attribute
	 * 0x148e0000 = iswanport | ppuinit | onumode | isolate |
	 * smac_learn | bit 26 (observed live on the factory system).
	 */
	ret = zx279133_spa_indirect_write(eth, 6, 1, 0x0000088f);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 6, 10, 0x01f4000c);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 6, 13, 0x00008008);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 6, 17, 0x0e014400);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 6, 18, 0x000001f1);
	if (ret)
		return ret;
	ret = zx279133_spa_indirect_write(eth, 6, 19, 0x148e0000);
	if (ret)
		return ret;
	{
		const u32 mf6[ZX279133_SMMU0_WORDS] = {
			0, 0x0e014400, 0x000001f1, 0x148e0000,
		};

		ret = zx279133_mf_eram_entry_write(eth, 6, mf6);
	}
	if (ret)
		return ret;

	/*
	 * The factory's runtime pkt-type table holds only the four
	 * entries below (0x00f7c000); the SPA uses them to tag CPU TX
	 * frames with the forwarding flow instead of the CPU exception
	 * flow.
	 */
	ret = zx279133_spa_pkt_type_runtime(eth);
	if (ret)
		return ret;

	/* spa_set_down_port_config(6): NPPT 0x8000 bits [11:6]. */
	reg = readl(eth->base + ZX279133_SPA_IPV6_CRC_MODE);
	writel((reg & ~(GENMASK(11, 6) | BIT(27))) | 6 << 6,
	       eth->base + ZX279133_SPA_IPV6_CRC_MODE);

	/*
	 * nppt_glb_lan_up_port_set(6) + nppt_glb_lan_up_en_set(1) +
	 * the OAM enables (mpcp/epon_oam/gpon_oam/omci_plus/omci,
	 * np_nppu_init): the CPU133 port table's byte for PPU port 6 is
	 * zero, so the port helper writes value 6 (not 0x106) into the
	 * NPPT 0x10 field; the enable helper sets bit 3 and the OAM
	 * helpers set bits [19:15].
	 */
	reg = readl(eth->base + 0x10);
	reg = (reg & ~0x107) | 0x006 | BIT(3) | GENMASK(19, 15);
	writel(reg, eth->base + 0x10);

	return 0;
}

/*
 * SE core SDT creation recovered from np.ko se_sdt_core_sdt_init() over
 * g_core_sdt_info_133. The ERAM pool starts at block 12288 (4096 age
 * blocks plus 8192 blocks skipped for the QMG) and the allocator is
 * strictly sequential. ERAM (type 1), hash (type 3), statistics (type 4),
 * and WRAM (type 5) descriptors are created below.
 */
static const u32 zx279133_core_sdt_info[][4] = {
	{ 0x0,  1, 0x3, 0x1000 },  { 0x1,  1, 0x3, 0x1 },
	{ 0x37, 1, 0x3, 0x1 },     { 0x2,  1, 0x3, 0x20 },
	{ 0x14, 1, 0x3, 0x20 },    { 0x15, 1, 0x3, 0x20 },
	{ 0x19, 1, 0x2, 0x20 },    { 0x40, 1, 0x3, 0x20 },
	{ 0x41, 1, 0x3, 0x20 },    { 0x42, 1, 0x3, 0x20 },
	{ 0x2e, 5, 0x3, 0x100 },   { 0x2f, 5, 0x3, 0x100 },
	{ 0x32, 1, 0x3, 0x40 },    { 0x3,  1, 0x1, 0x4000 },
	{ 0x4,  1, 0x7, 0x2000 },  { 0x8,  1, 0x0, 0x10000 },
	{ 0x16, 1, 0x3, 0x20 },    { 0x18, 1, 0x3, 0x20 },
	{ 0xf,  1, 0x3, 0x20 },    { 0x10, 1, 0x3, 0x20 },
	{ 0x11, 1, 0x3, 0x20 },    { 0x27, 1, 0x6, 0x400 },
	{ 0x6,  1, 0x3, 0x40 },    { 0x28, 1, 0x2, 0x40 },
	{ 0x29, 1, 0x3, 0x20 },    { 0x2a, 1, 0x3, 0x20 },
	{ 0x2c, 1, 0x3, 0x1000 },  { 0x44, 1, 0x3, 0x20 },
	{ 0x45, 1, 0x3, 0x20 },    { 0x46, 1, 0x3, 0x20 },
	{ 0x9,  3, 0x8, 0x64 },    { 0xa,  3, 0xa, 0x94 },
	{ 0xb,  3, 0x22, 0xd4 },   { 0xd,  3, 0x30, 0xd8 },
	{ 0xe,  3, 0x10, 0xed },   { 0x2b, 3, 0x10, 0xb8 },
	{ 0x43, 3, 0x30, 0xd8 },   { 0x2d, 3, 0x2, 0x40 },
	{ 0x1e, 3, 0x6, 0x1000054 }, { 0x33, 3, 0x6, 0x54 },
	{ 0x21, 3, 0x4, 0x44 },    { 0x24, 3, 0x4, 0x40 },
	{ 0xc,  3, 0x8, 0x45 },    { 0x13, 3, 0x4, 0x41 },
	{ 0x1a, 4, 0x1, 0x60000000 }, { 0x1b, 4, 0xc0, 0x40000000 },
	{ 0x1c, 4, 0x40, 0x40000000 }, { 0x1d, 4, 0x800, 0x40000000 },
	{ 0x38, 4, 0x80, 0x40000000 }, { 0x3b, 4, 0x40, 0x40000000 },
	{ 0x3c, 4, 0x40, 0x40000000 }, { 0x3d, 4, 0x40, 0x40000000 },
	{ 0x3e, 4, 0x800, 0x40000000 }, { 0x3f, 4, 0x800, 0x40000000 },
	{ 0x1f, 4, 0xc, 0x20000000 }, { 0x20, 4, 0x2, 0x40000000 },
	{ 0x22, 4, 0x88, 0x20000000 }, { 0x23, 4, 0x22, 0x40000000 },
	{ 0x25, 4, 0x20, 0x40000000 }, { 0x26, 4, 0x10, 0x20000000 },
	{ 0x12, 4, 0x8, 0x20000000 }, { 0x31, 4, 0x8, 0x20000000 },
	{ 0x64, 1, 0x2, 0x1 },     { 0x65, 1, 0x3, 0x21 },
	{ 0x66, 1, 0x1, 0x40 },    { 0x67, 1, 0x2, 0x4b0 },
	{ 0x68, 1, 0x1, 0x40 },
	{ 0x6e, 3, 0x6, 0x88 },    { 0x6b, 3, 0x4, 0x45 },
	{ 0x6f, 3, 0x4, 0x44 },
	{ 0x69, 4, 0xa0, 0x40000000 },
};

static const u32 zx279133_se_sdt_bit_len[8] = {
	1, 32, 64, 128, 2, 4, 8, 16
};

static int zx279133_se_sdt_eram_write(struct zx279133_eth *eth, u32 sdt_no,
				      u32 depth, u32 base, u32 mode)
{
	u32 node[2];

	node[0] = depth << 2;
	node[1] = (16 * (base & 0xffff)) |
		  ((0x10 | (mode << 1)) << 24);

	return zx279133_smmu0_write(eth, sdt_no, node, 2, 0xa8000000);
}

static int zx279133_se_parser_write(struct zx279133_eth *eth, u32 cmd,
				    const u32 *data, u32 words)
{
	u32 agclk = (readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) >> 1) & 1;
	u32 type = (cmd >> 9) & 7;
	u32 status, reg;
	int ret, i;

	if (agclk)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) & ~BIT(1),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	ret = readl_poll_timeout_atomic(eth->pps_base + 0x40000, status,
					status & BIT(type), 1, 1000);
	if (!ret) {
		for (i = 0; i < words; i++)
			writel(data[i], eth->pps_base + 0x40040 + 4 * i);
		reg = readl(eth->pps_base + 0x40004);
		writel((cmd & 0xffff) | (reg & ~0xffff),
		       eth->pps_base + 0x40004);
	}

	if (agclk)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) | BIT(1),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	return ret;
}

static int zx279133_se_parser_sdt_write(struct zx279133_eth *eth,
					u32 sdt_no, const u32 *node)
{
	return zx279133_se_parser_write(eth, sdt_no | 0x8000, node, 2);
}

static int zx279133_se_stat_sdt_write(struct zx279133_eth *eth, u32 sdt_no,
				      const u32 *node)
{
	u32 agclk = (readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) >> 2) & 1;
	u32 reg;
	int ret = 0;

	if (agclk)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) & ~BIT(2),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	writel(node[0], eth->pps_base + 0x58808);
	writel(node[1], eth->pps_base + 0x5880c);
	writel((sdt_no & 0xff) | 0x300, eth->pps_base + 0x58804);
	ret = readl_poll_timeout_atomic(eth->pps_base + 0x58818, reg, reg != 0,
					1, 1000);

	if (agclk)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) | BIT(2),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	return ret;
}

static int zx279133_se_hash_sdt_create(struct zx279133_eth *eth, u32 sdt_no,
				       u32 key_len, u32 tf)
{
	static const u32 key_base_tab[4] = { 4, 1, 2, 4 };
	u32 low = tf & 0xff;
	u32 width = (low >> 6) & 3;
	u32 as_rsp = (low >> 2) & 3;
	u32 multi = (tf >> 4) & 1;
	u32 age = (tf >> 5) & 1;
	u32 node[2], data = 0;
	u32 t, field;
	int ret, i;

	if (multi) {
		t = ffs(~eth->se_multi_idx_map) - 1;
		if (t >= 32)
			return -ENOSPC;
		eth->se_multi_idx_map |= BIT(t);
		t <<= 4;
		field = (t >> 5) & 0xf;
	} else {
		u32 key_base = key_base_tab[width];

		if (!key_len || ((key_base - 1) & key_len))
			return -EINVAL;
		t = ffs(~eth->se_hash_id_map[width]) - 1;
		if (t >= 32)
			return -ENOSPC;
		eth->se_hash_id_map[width] |= BIT(t);
		field = (key_len / key_base) & 0xf;
	}

	if (age && !eth->se_age_done) {
		/*
		 * se_hash_creat_age_sdt: one 1-bit x (0x1000<<7) age table
		 * at age pool block 0 (depth 4096); 32 parser base-address
		 * writes, each with the age base (0).
		 */
		for (i = 0; i < 32; i++) {
			ret = zx279133_se_parser_write(eth, i | 0x8400,
						       &data, 1);
			if (ret)
				return ret;
		}
		eth->se_age_done = true;
	}

	node[0] = ((age << 3) | (multi << 2) | (((tf >> 24) & 1) << 4)) << 8;
	node[1] = 0x30000000 |
		  (((field << 3) | ((t >> 2) & 7)) << 16) |
		  ((width & 3) << 23) | ((tf & 1) << 25) |
		  (((as_rsp << 4) | ((t & 3) << 6)) << 8);

	ret = zx279133_se_parser_write(eth, sdt_no | 0x8000, node, 2);
	if (ret)
		return ret;

	/* hash_multi_index_info_hw_set_default(): zero the HW index info. */
	if (multi)
		return zx279133_se_parser_write(eth, t | 0x8600, &data, 1);

	return 0;
}

static int zx279133_se_fast_multi_prepare(struct zx279133_eth *eth)
{
	static const u32 config[] = {
		0xff000000, 0xffffffff, 0xffffffff, 0xffffffff,
		0, 0, 0, 0, 0, 0, 0, 0,
		0x01000344,
	};
	u32 index = 0x8000;
	int ret;

	/* SDT 43 starts at multi-hash index 48.  Config table 0 extracts the
	 * 16-byte IPv4 fast-flow key and selects 256-bit hash table ID 1.
	 */
	ret = zx279133_se_parser_write(eth, 0x8800, config,
				       ARRAY_SIZE(config));
	if (ret)
		return ret;

	return zx279133_se_parser_write(eth, 0x8630, &index, 1);
}

static int zx279133_se_plcr_write(struct zx279133_eth *eth, u32 addr,
				  u32 value)
{
	u32 agclk = (readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) >> 2) & 1;

	if (agclk)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) & ~BIT(2),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	writel(addr & 0xffffff, eth->pps_base + 0x58004);
	writel(value, eth->pps_base + 0x58008);
	writel(1, eth->pps_base + 0x58000);

	if (agclk)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) | BIT(2),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	return 0;
}

static int zx279133_se_plcr_read(struct zx279133_eth *eth, u32 addr,
				 u32 *value)
{
	u32 agclk = (readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) >> 2) & 1;

	if (agclk)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) & ~BIT(2),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	writel((addr & 0xffffff) | 0x80000000, eth->pps_base + 0x58004);
	writel(1, eth->pps_base + 0x58000);
	*value = readl(eth->pps_base + 0x5800c);

	if (agclk)
		writel(readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG) | BIT(2),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	return 0;
}

static int zx279133_se_plcr_init(struct zx279133_eth *eth)
{
	u32 status;
	int ret, i;

	/* 4096-flow policer table, mode 3, base block 40960 from the pool end. */
	ret = zx279133_se_plcr_write(eth, 0x280017, 40960);
	if (ret)
		return ret;
	ret = zx279133_se_plcr_write(eth, 0x280016, 3);
	if (ret)
		return ret;
	ret = zx279133_se_plcr_write(eth, 0x280018, 62500);
	if (ret)
		return ret;
	ret = zx279133_se_plcr_write(eth, 0x280015, 1);
	if (ret)
		return ret;
	ret = zx279133_se_plcr_write(eth, 0x30000e, 24);
	if (ret)
		return ret;
	ret = zx279133_se_plcr_write(eth, 0x300010, 8000);
	if (ret)
		return ret;
	ret = zx279133_se_plcr_write(eth, 0x28001f, 16000);
	if (ret)
		return ret;
	ret = zx279133_se_plcr_write(eth, 0x280014, 33000);
	if (ret)
		return ret;

	for (i = 0; i < 20; i++) {
		ret = zx279133_se_plcr_read(eth, 0x30000f, &status);
		if (ret)
			return ret;
		if (status & BIT(0))
			return 0;
	}

	return dev_err_probe(eth->dev, -ETIMEDOUT,
			     "SE PLCR init done status %#x\n", status);
}

static int zx279133_se_sdt_create(struct zx279133_eth *eth)
{
	u32 eram_base = 12288;
	u32 stat_base = 9 << 12;
	u32 depth, base, bit_len;
	u32 node[2];
	unsigned int i;
	int ret;

	/* se_age_manage_init: parser age table depth register. */
	writel(2047, eth->pps_base + 0x400c4);

	for (i = 0; i < ARRAY_SIZE(zx279133_core_sdt_info); i++) {
		u32 sdt_no = zx279133_core_sdt_info[i][0];
		u32 type = zx279133_core_sdt_info[i][1];
		u32 a = zx279133_core_sdt_info[i][2];
		u32 b = zx279133_core_sdt_info[i][3];

		switch (type) {
		case 1:
		case 5:
			bit_len = zx279133_se_sdt_bit_len[a & 7];
			depth = (bit_len * b + 127) >> 7;
			base = eram_base;
			eram_base += depth;
			ret = zx279133_se_sdt_eram_write(eth, sdt_no, depth,
							 base, a);
			if (ret)
				return dev_err_probe(eth->dev, ret,
					"SE ERAM SDT %#x write failed\n",
					sdt_no);
			if (type == 5) {
				node[0] = depth << 2;
				node[1] = (16 * (base & 0xffff)) |
					  ((0x10 | (a << 1)) << 24);
				ret = zx279133_se_parser_sdt_write(eth, sdt_no,
								   node);
				if (ret)
					return dev_err_probe(eth->dev, ret,
						"SE parser SDT %#x write failed\n",
						sdt_no);
			}
			break;
		case 3:
			ret = zx279133_se_hash_sdt_create(eth, sdt_no, a, b);
			if (ret)
				return dev_err_probe(eth->dev, ret,
					"SE hash SDT %#x create failed\n",
					sdt_no);
			break;
		case 4: {
			u32 st_mode = (b >> 24) >> 5;

			bit_len = zx279133_se_sdt_bit_len[st_mode];
			depth = (bit_len * a + 127) >> 7;
			base = stat_base;
			stat_base += depth;
			node[0] = depth << 12;
			node[1] = (base & 0xfffff) |
				  ((0x40 | (st_mode << 1)) << 24);
			ret = zx279133_se_stat_sdt_write(eth, sdt_no, node);
			if (ret)
				return dev_err_probe(eth->dev, ret,
					"SE stat SDT %#x write failed\n",
					sdt_no);
			break;
		}
		default:
			break;
		}
	}

	ret = zx279133_se_fast_multi_prepare(eth);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "SE fast multi-hash setup failed\n");

	/*
	 * The factory's runtime SE table contents captured live: sdt 6 (the
	 * 64-entry port table) and sdt 0x10 entry 4. The patterns below are
	 * the app's P2P-mode values; the microcode reads these directly.
	 */
	{
		static const struct {
			u32 blk, w0, w1, w2, w3;
		} tabs[] = {
			{ 22898 + 4, 0x54145414, 0x58185414, 0x581a4014, 0x581a581a },
			{ 23026 + 1, 0xffff0140, 0x7801ffff, 0, 0 },
			{ 23026 + 2, 0xffff0140, 0x7801ffff, 0, 0 },
			{ 23026 + 3, 0xffff0140, 0x7801ffff, 0, 0 },
			{ 23026 + 4, 0xffff0140, 0x7801ffff, 0, 0 },
			{ 23026 + 5, 0xffff0140, 0x7801ffff, 0, 0 },
			{ 23026 + 6, 0x00000040, 0, 0, 0 },
			{ 23026 + 7, 0xffff0140, 0x7801ffff, 0, 0 },
			{ 23026 + 8, 0, 0, 0, 0x80000000 },
		};
		u32 idx;

		for (idx = 0; idx < ARRAY_SIZE(tabs); idx++) {
			u32 node[4] = { tabs[idx].w0, tabs[idx].w1,
					tabs[idx].w2, tabs[idx].w3 };

			ret = zx279133_smmu0_write(eth, tabs[idx].blk << 7,
						   node, 4,
						   ZX279133_SMMU0_CMD_WRITE);
			if (ret)
				return ret;
		}
		for (idx = 0; idx < 16; idx++) {
			u32 node[4] = { 0x10000 << idx, 0, 0, 0 };

			ret = zx279133_smmu0_write(eth,
						   (23026 + 16 + idx) << 7,
						   node, 4,
						   ZX279133_SMMU0_CMD_WRITE);
			if (ret)
				return ret;
		}
		for (idx = 0; idx < 16; idx++) {
			u32 node[4] = { 0, 1 << idx, 0, 0 };

			ret = zx279133_smmu0_write(eth,
						   (23026 + 32 + idx) << 7,
						   node, 4,
						   ZX279133_SMMU0_CMD_WRITE);
			if (ret)
				return ret;
		}
		for (idx = 0; idx < 4; idx++) {
			u32 node[4] = { 0, 0, 0, 0x82000000 };

			ret = zx279133_smmu0_write(eth,
						   (23026 + 59 + idx) << 7,
						   node, 4,
						   ZX279133_SMMU0_CMD_WRITE);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static void zx279133_se_stat_runtime_prepare(struct zx279133_eth *eth)
{
	u32 agclk = readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG);
	u32 value;

	/* stat_init()'s isCpuType_133() path enables every stat RAM client. */
	if (agclk & BIT(2))
		writel(agclk & ~BIT(2),
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	value = readl(eth->pps_base + ZX279133_SE_STAT_RUNTIME_CFG);
	writel(value | ZX279133_SE_STAT_RUNTIME_MASK,
	       eth->pps_base + ZX279133_SE_STAT_RUNTIME_CFG);

	if (agclk & BIT(2))
		writel(agclk, eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	value = readl(eth->pps_base + ZX279133_SE_PARSER_G988_CFG);
	writel(value | BIT(1), eth->pps_base + ZX279133_SE_PARSER_G988_CFG);
}

static int zx279133_ppu_dup_entry(struct zx279133_eth *eth, u32 index,
				  u32 value)
{
	u32 agclk;
	u32 status;
	int ret;

	agclk = readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG);
	if (agclk & ZX279133_PPU_REORDER_AGCLK)
		writel(agclk & ~ZX279133_PPU_REORDER_AGCLK,
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	ret = readl_poll_timeout_atomic(eth->pps_base +
					ZX279133_PPU_BLOCK_RDY,
					status, status & BIT(0), 1, 1000);
	if (ret) {
		dev_err_probe(eth->dev, ret,
			      "PPU block not ready: %#x\n", status);
		goto out_gate;
	}
	writel((readl(eth->pps_base + ZX279133_PPU_BLOCK_DATA) &
		~ZX279133_PPU_BLOCK_DATA_MASK) | value,
		eth->pps_base + ZX279133_PPU_BLOCK_DATA);
	writel((readl(eth->pps_base + ZX279133_PPU_BLOCK_INDEX) &
		~ZX279133_PPU_BLOCK_INDEX_MASK) | index,
		eth->pps_base + ZX279133_PPU_BLOCK_INDEX);

out_gate:
	if (agclk & ZX279133_PPU_REORDER_AGCLK)
		writel(agclk, eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	return ret;
}

static int zx279133_ppu_mcode_prepare(struct zx279133_eth *eth)
{
	const struct firmware *fw;
	const u8 *p;
	const u8 *end;
	u32 lo[4], hi[4];
	u32 first, inst_num, tag, value, entry, pkt_type, i;
	u32 agclk;
	int ret;

	ret = request_firmware(&fw, "zte/zx279133/mcode_intel.bin", eth->dev);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "failed to load PPU microcode\n");

	if (fw->size < 48 ||
	    get_unaligned_be32(fw->data) != ZX279133_MCODE_MAGIC ||
	    get_unaligned_be32(fw->data + 8) != ZX279133_MCODE_INST_NUM)
		goto err_fw;

	inst_num = get_unaligned_be32(fw->data + 8);
	first = get_unaligned_be32(fw->data + 12);
	if (!first || first > ZX279133_MCODE_INST_NUM - 80 ||
	    first * 8 + 16 > fw->size || inst_num * 8 < 624 ||
	    inst_num * 8 + 16 > fw->size)
		goto err_fw;
	first = ALIGN(first, 4);

	agclk = readl(eth->pps_base + ZX279133_PPU_AGCLK_CFG);
	if (agclk & ZX279133_PPU_CORE_AGCLK)
		writel(agclk & ~ZX279133_PPU_CORE_AGCLK,
		       eth->pps_base + ZX279133_PPU_AGCLK_CFG);

	p = fw->data + 16;
	for (i = 0; i < first; i += 4) {
		for (entry = 0; entry < 4; entry++) {
			/* np_downlaod_mcode stores each file pair high, low. */
			hi[entry] = get_unaligned_be32(p + 8 * entry);
			lo[entry] = get_unaligned_be32(p + 8 * entry + 4);
		}
		ret = zx279133_ppu_inst_block(eth, i, lo, hi);
		if (ret)
			goto out_gate;
		p += 32;
	}

	writel((readl(eth->pps_base + ZX279133_PPU_IKEY_AFULL) &
		~ZX279133_PPU_IKEY_AFULL_MASK),
	       eth->pps_base + ZX279133_PPU_IKEY_AFULL);

	p = fw->data + 8 * inst_num - 624;
	for (i = 0; i < 80; i += 4) {
		for (entry = 0; entry < 4; entry++) {
			hi[entry] = get_unaligned_be32(p + 8 * entry);
			lo[entry] = get_unaligned_be32(p + 8 * entry + 4);
		}
		ret = zx279133_ppu_inst_block(eth, 12208 + i, lo, hi);
		if (ret)
			goto out_gate;
		p += 32;
	}

	p = fw->data + 8 * inst_num + 16;
	end = fw->data + fw->size;
	pkt_type = 0;
	while (p + 8 <= end) {
		tag = get_unaligned_be32(p);
		value = get_unaligned_be32(p + 4);
		switch (tag) {
		case ZX279133_MCODE_TAG_PKT_TYPE:
			if (pkt_type >= 512)
				goto err_layout;
			entry = (value & 0x3fff) |
				(value >> 16 & 0x3f) << 14 |
				(value >> 22 & 0x7) << 20 |
				(value & BIT(25) ? BIT(23) : 0);
			if (!(value & 0x2000000))
				entry = 0;
			ret = zx279133_spa_indirect_write(eth, 64, pkt_type,
							  entry);
			if (ret)
				goto out_gate;
			pkt_type++;
			break;
		case ZX279133_MCODE_TAG_DUP:
			entry = value & BIT(23);
			if (value & BIT(23))
				entry |= ((value & BIT(22)) ? BIT(22) : 0) |
					(value & 0xffff) << 6 |
					(value >> 16 & 0x3f);
			ret = zx279133_ppu_dup_entry(eth, value >> 24, entry);
			if (ret)
				goto out_gate;
			break;
		case ZX279133_MCODE_TAG_SKIP:
			p += 16;
			break;
		case ZX279133_MCODE_TAG_VERSION:
			if (p + 16 <= end)
				dev_info(eth->dev,
					 "PPU microcode version %#x.%#x\n",
					 value, get_unaligned_be32(p + 8));
			p += 8;
			break;
		case ZX279133_MCODE_TAG_PORT_FLOW:
			/*
			 * np_download_set_port_flow_table(): the tag updates
			 * SPA memory address 0 with four read-modify-write
			 * fields (attr 1 bits 14:1, attr 3 bits 20:15, attr 4
			 * bits 23:21, attr 0 bit 0). Apply them all in one
			 * read-modify-write cycle so they do not clobber each
			 * other.
			 */
			entry = value >> 26;
			if ((value & BIT(25)) && entry != 62) {
				u32 v;

				ret = zx279133_spa_indirect_read(eth, entry, 0,
								 &v);
				if (ret)
					goto out_gate;
				v &= ~(BIT(0) | GENMASK(14, 1) |
				       GENMASK(20, 15) | GENMASK(23, 21));
				v |= (value & 0x3fff) << 1;
				v |= (value >> 16 & 0x3f) << 15;
				v |= (value >> 22 & 0x7) << 21;
				v |= BIT(0);
				ret = zx279133_spa_indirect_write(eth, entry, 0, v);
				if (ret)
					goto out_gate;
			}
			break;
		default:
			break;
		}
		p += 8;
	}
	if (pkt_type != 512)
		goto err_layout;
	ret = 0;
	eth->ppu_mcode_prepared = true;

out_gate:
	writel(agclk, eth->pps_base + ZX279133_PPU_AGCLK_CFG);
	release_firmware(fw);
	return ret;

err_layout:
	ret = -EINVAL;
	goto out_gate;

err_fw:
	release_firmware(fw);
	return dev_err_probe(eth->dev, -EINVAL,
			     "invalid PPU microcode file\n");
}

static void zx279133_ppu_mcode_restore(struct zx279133_eth *eth)
{
	eth->ppu_mcode_prepared = false;
}

static const u32 zx279133_spa_tpid_values[] = {
	0x88a88100,
	0x92009100,
	0x07020701,
	0x81000703,
};

static void zx279133_spa_port_attr_init(struct zx279133_eth *eth)
{
	u32 value;
	int mem_id;

	/*
	 * np.ko spa_init() per-port attribute tables for CPU133:
	 * attr 42 writes 0x0A00000C (0xFA00000C & 0x0fffffff) to the low
	 * 28 bits of SPA address 10, attr 8 writes 6 on the CPU-side
	 * memories {10,12,14} and 7 elsewhere, attr 9 writes 0x111 (both
	 * at address 1), and the per-port default TPID selection
	 * (attrs 48-67) writes 0x8008 at address 13 (attrs 49 and 53 = 1,
	 * the rest 0) for all 64 memories plus memory 15; addresses 14
	 * and 15 stay 0. attr 4 writes 7 at address 0 for the PON-side
	 * memory ids 49-54.
	 */
	for (mem_id = 0; mem_id < 64; mem_id++) {
		if (!zx279133_spa_indirect_read(eth, mem_id, 10, &value)) {
			value = (value & ~GENMASK(27, 0)) | 0x0a00000c;
			zx279133_spa_indirect_write(eth, mem_id, 10, value);
		}

		if (!zx279133_spa_indirect_read(eth, mem_id, 1, &value)) {
			u32 attr8 = (mem_id == 10 || mem_id == 12 ||
				     mem_id == 14) ? 6 : 7;

			value = (value & ~GENMASK(11, 0)) |
				attr8 | 0x111 << 3;
			zx279133_spa_indirect_write(eth, mem_id, 1, value);
		}

		if (!zx279133_spa_indirect_read(eth, mem_id, 13, &value)) {
			value = (value & ~GENMASK(23, 0)) | 0x8008;
			zx279133_spa_indirect_write(eth, mem_id, 13, value);
		}
	}
	if (!zx279133_spa_indirect_read(eth, 15, 13, &value)) {
		value = (value & ~GENMASK(23, 0)) | 0x8008;
		zx279133_spa_indirect_write(eth, 15, 13, value);
	}

	for (mem_id = 49; mem_id < 55; mem_id++) {
		if (zx279133_spa_indirect_read(eth, mem_id, 0, &value))
			continue;
		value = (value & ~GENMASK(23, 21)) | 7 << 21;
		zx279133_spa_indirect_write(eth, mem_id, 0, value);
	}
}

static void zx279133_spa_prepare(struct zx279133_eth *eth)
{
	unsigned int i;

	eth->spa_saved[0] = readl(eth->base + ZX279133_SPA_TPID_BASE);
	eth->spa_saved[1] = readl(eth->base + ZX279133_SPA_TPID_BASE + 0x4);
	eth->spa_saved[2] = readl(eth->base + ZX279133_SPA_TPID_BASE + 0x8);
	eth->spa_saved[3] = readl(eth->base + ZX279133_SPA_TPID_BASE + 0xc);
	eth->spa_saved[4] = readl(eth->base + ZX279133_SPA_TRAP_ETH_TYPE);
	eth->spa_saved[5] = readl(eth->base + ZX279133_SPA_TRAP_ETH_TYPE + 0x4);
	eth->spa_saved[6] = readl(eth->base + ZX279133_SPA_TRAP_ETH_TYPE + 0x8);

	for (i = 0; i < ZX279133_SPA_ONU_COUNT; i++) {
		eth->spa_saved[7 + 2 * i] =
			readl(eth->base + ZX279133_SPA_ONU_MAC_HI +
			      ZX279133_SPA_ONU_STRIDE * i);
		eth->spa_saved[8 + 2 * i] =
			readl(eth->base + ZX279133_SPA_ONU_MAC_LO +
			      ZX279133_SPA_ONU_STRIDE * i);
	}
	for (i = 0; i < ZX279133_SPA_TRAP_DMAC_COUNT; i++) {
		eth->spa_saved[39 + 2 * i] =
			readl(eth->base + ZX279133_SPA_TRAP_DMAC_HI +
			      ZX279133_SPA_TRAP_DMAC_STRIDE * i);
		eth->spa_saved[40 + 2 * i] =
			readl(eth->base + ZX279133_SPA_TRAP_DMAC_LO +
			      ZX279133_SPA_TRAP_DMAC_STRIDE * i);
	}

	for (i = 0; i < ARRAY_SIZE(zx279133_spa_tpid_values); i++)
		writel(zx279133_spa_tpid_values[i],
		       eth->base + ZX279133_SPA_TPID_BASE + 4 * i);
	writel(0, eth->base + ZX279133_SPA_TRAP_ETH_TYPE);
	writel(0, eth->base + ZX279133_SPA_TRAP_ETH_TYPE + 0x4);
	writel(0, eth->base + ZX279133_SPA_TRAP_ETH_TYPE + 0x8);
	for (i = 0; i < ZX279133_SPA_ONU_COUNT; i++) {
		writel(ZX279133_SPA_MAC_HI_VALUE,
		       eth->base + ZX279133_SPA_ONU_MAC_HI +
		       ZX279133_SPA_ONU_STRIDE * i);
		writel((eth->spa_saved[8 + 2 * i] & ~ZX279133_SPA_MAC_LO_MASK) |
		       ZX279133_SPA_MAC_LO_VALUE,
		       eth->base + ZX279133_SPA_ONU_MAC_LO +
		       ZX279133_SPA_ONU_STRIDE * i);
	}
	zx279133_program_spa_cpu_mac(eth, eth->ndev->dev_addr);
	for (i = 0; i < ZX279133_SPA_TRAP_DMAC_COUNT; i++) {
		writel(ZX279133_SPA_MAC_HI_VALUE,
		       eth->base + ZX279133_SPA_TRAP_DMAC_HI +
		       ZX279133_SPA_TRAP_DMAC_STRIDE * i);
		writel((eth->spa_saved[40 + 2 * i] & ~ZX279133_SPA_MAC_LO_MASK) |
		       ZX279133_SPA_MAC_LO_VALUE,
		       eth->base + ZX279133_SPA_TRAP_DMAC_LO +
		       ZX279133_SPA_TRAP_DMAC_STRIDE * i);
	}
	/* spa_set_tcp_ctrl_mask(7): 0x8000 bits [19:12]. */
	writel((readl(eth->base + ZX279133_SPA_IPV6_CRC_MODE) &
		~(GENMASK(19, 12) | BIT(27))) | 7 << 12,
	       eth->base + ZX279133_SPA_IPV6_CRC_MODE);
	zx279133_spa_port_attr_init(eth);
	eth->spa_prepared = true;
}

static void zx279133_spa_restore(struct zx279133_eth *eth)
{
	unsigned int i;

	if (!eth->spa_prepared)
		return;

	for (i = 0; i < 4; i++)
		writel(eth->spa_saved[i],
		       eth->base + ZX279133_SPA_TPID_BASE + 4 * i);
	for (i = 0; i < 3; i++)
		writel(eth->spa_saved[4 + i],
		       eth->base + ZX279133_SPA_TRAP_ETH_TYPE + 4 * i);
	for (i = 0; i < ZX279133_SPA_ONU_COUNT; i++) {
		writel(eth->spa_saved[7 + 2 * i],
		       eth->base + ZX279133_SPA_ONU_MAC_HI +
		       ZX279133_SPA_ONU_STRIDE * i);
		writel(eth->spa_saved[8 + 2 * i],
		       eth->base + ZX279133_SPA_ONU_MAC_LO +
		       ZX279133_SPA_ONU_STRIDE * i);
	}
	for (i = 0; i < ZX279133_SPA_TRAP_DMAC_COUNT; i++) {
		writel(eth->spa_saved[39 + 2 * i],
		       eth->base + ZX279133_SPA_TRAP_DMAC_HI +
		       ZX279133_SPA_TRAP_DMAC_STRIDE * i);
		writel(eth->spa_saved[40 + 2 * i],
		       eth->base + ZX279133_SPA_TRAP_DMAC_LO +
		       ZX279133_SPA_TRAP_DMAC_STRIDE * i);
	}
	eth->spa_prepared = false;
}

static int zx279133_nppu_early_prepare(struct zx279133_eth *eth)
{
	u32 value;
	int ret;

	eth->nppu_early_saved[0] = readl(eth->base + ZX279133_ISU_CFG);
	eth->nppu_early_saved[1] = readl(eth->base + ZX279133_ISU_INIT_REQ);
	eth->nppu_early_saved[2] = readl(eth->base + ZX279133_ISU_DWRR_WEIGHT);
	eth->nppu_early_saved[3] = readl(eth->base + ZX279133_ISU_RING_GAP);
	eth->nppu_early_saved[4] = readl(eth->base + ZX279133_ODMA_ISU_SP_EN);

	ret = readl_poll_timeout_atomic(eth->base + ZX279133_ISU_INIT_REQ,
					value,
					!(value & ZX279133_ISU_INIT_REQ_MASK),
					1, 1000);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "ISU init request remained busy: %#x\n",
				     value);
	writel((value & ~ZX279133_ISU_INIT_REQ_MASK) |
	       ZX279133_ISU_INIT_REQ_VALUE,
	       eth->base + ZX279133_ISU_INIT_REQ);
	writel((eth->nppu_early_saved[0] & ~ZX279133_ISU_CFG_MASK) |
	       ZX279133_ISU_SPA_SP_EN,
	       eth->base + ZX279133_ISU_CFG);
	writel(ZX279133_ISU_DWRR_WEIGHT_VALUE,
	       eth->base + ZX279133_ISU_DWRR_WEIGHT);
	writel((eth->nppu_early_saved[3] & ~ZX279133_ISU_RING_GAP_MASK) |
	       ZX279133_ISU_RING_GAP_VALUE,
	       eth->base + ZX279133_ISU_RING_GAP);
	writel(eth->nppu_early_saved[4] | ZX279133_ODMA_ISU_SP_EN_BIT,
	       eth->base + ZX279133_ODMA_ISU_SP_EN);
	eth->nppu_early_prepared = true;

	ret = readl_poll_timeout_atomic(eth->base + ZX279133_ISU_INIT_REQ,
					value,
					!(value & ZX279133_ISU_INIT_REQ_MASK),
					1, 1000);
	if (ret)
		return dev_err_probe(eth->dev, ret,
				     "ISU init request timed out: %#x\n", value);

	return 0;
}

static void zx279133_nppu_early_restore(struct zx279133_eth *eth)
{
	if (!eth->nppu_early_prepared)
		return;

	writel(eth->nppu_early_saved[0], eth->base + ZX279133_ISU_CFG);
	writel(eth->nppu_early_saved[1] & ~ZX279133_ISU_INIT_REQ_MASK,
	       eth->base + ZX279133_ISU_INIT_REQ);
	writel(eth->nppu_early_saved[2], eth->base + ZX279133_ISU_DWRR_WEIGHT);
	writel(eth->nppu_early_saved[3], eth->base + ZX279133_ISU_RING_GAP);
	writel(eth->nppu_early_saved[4], eth->base + ZX279133_ODMA_ISU_SP_EN);
	eth->nppu_early_prepared = false;
}

static void zx279133_nppu_late_prepare(struct zx279133_eth *eth)
{
	eth->nppu_late_saved[0] = readl(eth->base + ZX279133_SPA_IPV6_CRC_MODE);
	eth->nppu_late_saved[1] = readl(eth->base + ZX279133_GLB_OAM_EN);

	writel((eth->nppu_late_saved[0] &
		~(ZX279133_SPA_IPV6_CRC_BIT | ZX279133_SPA_TCP_CTRL_MASK)) |
	       ZX279133_SPA_TCP_CTRL_VALUE << 12,
	       eth->base + ZX279133_SPA_IPV6_CRC_MODE);
	writel((eth->nppu_late_saved[1] & ~ZX279133_GLB_OAM_MASK) |
	       ZX279133_GLB_OAM_VALUE,
	       eth->base + ZX279133_GLB_OAM_EN);
	eth->nppu_late_prepared = true;
}

static void zx279133_nppu_late_restore(struct zx279133_eth *eth)
{
	if (!eth->nppu_late_prepared)
		return;

	writel(eth->nppu_late_saved[0], eth->base + ZX279133_SPA_IPV6_CRC_MODE);
	writel(eth->nppu_late_saved[1], eth->base + ZX279133_GLB_OAM_EN);
	eth->nppu_late_prepared = false;
}

static void zx279133_sdet_prepare(struct zx279133_eth *eth)
{
	eth->sdet_saved[0] = readl(eth->base + ZX279133_SDET_FRAME_CFG);
	eth->sdet_saved[1] = readl(eth->base + ZX279133_SDET_DOWN_FRAME_CFG);

	writel((eth->sdet_saved[0] & GENMASK(31, 28)) |
	       ZX279133_SDET_UP_MAX_LEN << ZX279133_SDET_UP_MAX_SHIFT |
	       ZX279133_SDET_UP_MIN_LEN,
	       eth->base + ZX279133_SDET_FRAME_CFG);
	writel((eth->sdet_saved[1] & GENMASK(31, 30)) |
	       ZX279133_SDET_DOWN_MAX_LEN << ZX279133_SDET_DOWN_MAX_SHIFT |
	       ZX279133_SDET_DOWN_MIN_LEN,
	       eth->base + ZX279133_SDET_DOWN_FRAME_CFG);
	eth->sdet_prepared = true;
}

static void zx279133_sdet_restore(struct zx279133_eth *eth)
{
	if (!eth->sdet_prepared)
		return;

	writel(eth->sdet_saved[0], eth->base + ZX279133_SDET_FRAME_CFG);
	writel(eth->sdet_saved[1], eth->base + ZX279133_SDET_DOWN_FRAME_CFG);
	eth->sdet_prepared = false;
}

static void zx279133_sipc_final_prepare(struct zx279133_eth *eth)
{
	/* sipc_set_sch_mode_sw(0): clear the scheduler-mode-switch bit. */
	writel(readl(eth->base + ZX279133_SIPC_CFG) &
	       ~ZX279133_SIPC_SCH_MODE_SW,
	       eth->base + ZX279133_SIPC_CFG);
	eth->sipc_final_prepared = true;
}

static void zx279133_sipc_final_restore(struct zx279133_eth *eth)
{
	if (!eth->sipc_final_prepared)
		return;

	writel(eth->sipc_saved[0], eth->base + ZX279133_SIPC_CFG);
	eth->sipc_final_prepared = false;
}

int zx279133_np_prepare(struct zx279133_eth *eth)
{
	int ret;

	ret = zx279133_se_plcr_init(eth);
	if (ret)
		return ret;
	ret = zx279133_se_sdt_create(eth);
	if (ret)
		return ret;
	zx279133_se_stat_runtime_prepare(eth);
	ret = zx279133_ppu_mcode_prepare(eth);
	if (ret)
		return ret;
	ret = zx279133_np_ppu_init_tail(eth);
	if (ret)
		return ret;
	zx279133_spa_prepare(eth);
	ret = zx279133_nppu_early_prepare(eth);
	if (ret)
		goto err_nppu_early_restore;
	zx279133_smct_prepare(eth);
	zx279133_nppu_late_prepare(eth);
	zx279133_sdet_prepare(eth);
	zx279133_sipc_final_prepare(eth);
	ret = zx279133_mf_port_ppuinit(eth);
	if (ret)
		goto err_route_off;

	return 0;

err_route_off:
	zx279133_route_set(eth, false);
	zx279133_sipc_final_restore(eth);
	zx279133_sdet_restore(eth);
	zx279133_nppu_late_restore(eth);
	zx279133_smct_restore(eth);
err_nppu_early_restore:
	zx279133_nppu_early_restore(eth);
	zx279133_spa_restore(eth);
	zx279133_ppu_cluster_runtime_restore(eth);
	zx279133_ppu_mcode_restore(eth);
	return ret;
}

void zx279133_np_restore(struct zx279133_eth *eth)
{
	zx279133_sipc_final_restore(eth);
	zx279133_sdet_restore(eth);
	zx279133_nppu_late_restore(eth);
	zx279133_smct_restore(eth);
	zx279133_nppu_early_restore(eth);
	zx279133_spa_restore(eth);
	zx279133_ppu_cluster_runtime_restore(eth);
	zx279133_ppu_mcode_restore(eth);
}
