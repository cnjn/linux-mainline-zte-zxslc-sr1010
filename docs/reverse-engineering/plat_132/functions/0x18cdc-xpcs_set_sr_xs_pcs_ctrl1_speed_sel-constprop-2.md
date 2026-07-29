# 0x18cdc xpcs_set_sr_xs_pcs_ctrl1_speed_sel.constprop.2

Status: complete. The byte selector chooses the standard direct/base-relative
PCS window at offset `0x0c0000`; the function clears bit 13 with
`control &= 0xffffdfffU` and returns no meaningful value. Complete 29-instruction
ARM64 body and two caller xrefs (`xpcs_1g_mode_conf`, HSGMII configuration)
verified. IDA type is recovered as `void (u8 xmac)`.
