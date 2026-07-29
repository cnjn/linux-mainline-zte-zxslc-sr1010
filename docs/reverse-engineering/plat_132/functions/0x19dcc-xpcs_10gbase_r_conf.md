# 0x19dcc xpcs_10gbase_r_conf

Status: complete. Valid selectors through four prepare transition target zero,
write PCS type zero, pulse SR-XS/PCS low power around delay `859000`, and cache
mode zero. Invalid selectors log and return `-1`; success returns zero. Complete
32-instruction ARM64 body and two `xmac_10gbase_r_conf` caller xrefs verified.
IDA type is `int xpcs_10gbase_r_conf(u8 xmac)`.
