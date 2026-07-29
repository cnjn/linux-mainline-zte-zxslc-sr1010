# 0x19f74 xpcs_1000base_x_conf

Status: complete. Valid selectors prepare target mode two, invoke
`xpcs_1g_mode_conf(xmac, speed, duplex, 0)`, cache mode two regardless of that
call's status, and return it. Invalid selectors log and return `-1`. Two
`xmac_1gbase_x_conf` callers and the recovered ABI were verified.
