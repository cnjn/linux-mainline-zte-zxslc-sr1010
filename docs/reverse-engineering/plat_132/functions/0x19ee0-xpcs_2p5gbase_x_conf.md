# 0x19ee0 xpcs_2p5gbase_x_conf

Status: complete. Valid selectors through four call mode-transition preparation
with target four, write PCS type `0x0e`, clear the fixed PMA speed-select bit,
enable PMA low power, delay `859000`, disable low power, and cache mode four.
Invalid selectors log and return `-1`; successful configuration returns zero.
Both direct calls arise from `xmac_2pt5gbase_x_conf`. IDA type is
`int xpcs_2p5gbase_x_conf(u8 xmac)`.
