# 0x18d50 xpcs_set_sr_pma_ctrl_speed_sel.constprop.3

Status: complete. The byte selector chooses the standard direct/base-relative
PCS window at offset `0x040000`; the function clears bit 13 with
`control &= 0xffffdfffU` and returns no meaningful value. Complete 29-instruction
ARM64 body and three caller xrefs (1G, 2.5GBASE-X, HSGMII configuration)
verified. IDA type is recovered as `void (u8 xmac)`.
