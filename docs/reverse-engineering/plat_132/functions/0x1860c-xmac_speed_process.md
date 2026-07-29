# 0x1860c xmac_speed_process

## Status

- Status: complete
- Confidence: verified auto gate, mode routing, both PCS query paths, output
  initialization/order, speed writes/logging, void return, and sole caller.
- Size: `0x140` bytes, 78 ARM64 instructions.
- Recovered signature: `void xmac_speed_process(u8 xmac)`.

## Semantics

The function initializes local `uni_speed`, `duplex`, and `auto_status` words
to zero and the mapped XMAC-speed word to one. It first requires a nonzero
`g_xmac_work_in_auto[xmac]` byte; otherwise it returns immediately.

It reads `sg_xmac_work_mode[xmac]` and selects one inline path:

- Mode `3`: queries `xpcs_get_speed_duplex_in_auto_en_sgmii_mode`.
- Raw unsigned modes `5` through `7`: queries
  `xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode`.
- Every other mode: returns without a side effect.

Both query paths require return value zero and `auto_status == 1`. They then map
the returned UNI speed, write it through `xmac_set_speed_sel`, and emit the
protocol-specific SGMII or USXGMII speed log. The duplex output is not consumed
after the query. An unsupported speed keeps the initialized mapped-speed value
one because the mapping helper's default path does not write its output.

## Caller Context

The sole direct caller is `phy_zxic051_check @ 0x1c0c0` at `0x1c2c0`, after its
PHY link and NPPT global-link checks succeed and before it reads current UNI
speed for mismatch reconciliation.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The function reads
shared XMAC state arrays and delegates the volatile register write.

## Evidence

- Complete 78-instruction ARM64 body at `0x1860c` through `0x18748`.
- Exact auto-flag byte gate, mode-three equality branch, and raw unsigned
  mode-five-through-seven range branch.
- Both PCS calls use verified `(xmac, &speed, &duplex, &auto_status)` ABI order.
- Exact two speed-map/write/log tails and one direct caller xref.
- IDA function type updated at `0x1860c` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Why the generic helper duplicates rather than calls the two narrower helpers.
- Exact hardware semantics of the shared auto flag and auto-status outputs.
