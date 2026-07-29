# 0x19a90 xpcs_auto_negotiation_conf_in_1000base_x_mode

Status: complete. Accepts `(u8 xmac, u8 auto_enable, u8 enable_2p5g)`, rejects
invalid selectors and cached PCS modes other than three, then configures AN
controls and paired 2.5G flags. `enable_2p5g == 1` writes timer `1953` and CL37
recovered `int` signature were verified; no direct caller xref exists.
