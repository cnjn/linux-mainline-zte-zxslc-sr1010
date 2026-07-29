# 0x1a19c xpcs_sgmii_mode_conf

Status: complete. Valid selectors prepare target mode three, invoke
`xpcs_1g_mode_conf(xmac, mode_value, config_value, 2)`, cache mode three
regardless of that call's status, and return it. Invalid selectors log and
return `-1`. Both callers and the recovered `(u8, u32, u32)` ABI were verified.
