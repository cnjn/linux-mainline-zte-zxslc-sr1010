# 0x19e54 xpcs_5gbase_r_conf

Status: complete. Valid selectors through four prepare transition target one,
write type `0x0f`, enable SR-XS/PCS low power, delay `859000`, clear low power,
and cache mode one. Invalid selectors log and return `-1`; success returns zero.
The full 34-instruction ARM64 body and two caller xrefs were verified. IDA type
is `int xpcs_5gbase_r_conf(u8 xmac)`.
