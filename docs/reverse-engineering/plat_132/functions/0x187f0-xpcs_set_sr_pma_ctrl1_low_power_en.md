# 0x187f0 xpcs_set_sr_pma_ctrl1_low_power_en

Status: complete. The byte selector chooses the direct/base-relative PCS window
at offset `0x040000`; the function replaces bit 11 with enable bit zero using
`(control & 0xfffff7ffU) | ((enable & 1U) << 11)`. Complete 32-instruction ARM64
body and two direct calls from `xpcs_2p5gbase_x_conf` verified. IDA type is
`void (u8 xmac, u8 enable)`.
