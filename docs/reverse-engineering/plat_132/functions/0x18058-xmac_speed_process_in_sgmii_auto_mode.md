# 0x18058 xmac_speed_process_in_sgmii_auto_mode

## Status

- Status: complete
- Confidence: verified all four gates, exact PCS output-pointer order, default
  output values, speed conversion/write, logging, void return, and sole caller.
- Size: `0xd8` bytes, 52 ARM64 instructions.
- Recovered signature:
  `void xmac_speed_process_in_sgmii_auto_mode(u8 xmac)`.

## Semantics

The helper initializes local `uni_speed`, `duplex`, and `auto_status` words to
zero and initializes the mapped XMAC-speed word to one. It returns without a
side effect unless all conditions hold:

1. `g_xmac_work_in_auto[xmac]` is nonzero.
2. `sg_xmac_work_mode[xmac] == 3`.
3. `xpcs_get_speed_duplex_in_auto_en_sgmii_mode(xmac, &uni_speed, &duplex,
   &auto_status)` returns zero.
4. `auto_status == 1`.

On success it maps `uni_speed` through
`xmac_switch_uni_speed_to_xmac_speed`, writes the result through
`xmac_set_speed_sel`, and logs both speed values. The `duplex` output is
obtained but not otherwise consumed here. If the speed conversion receives an
unsupported value, its no-write behavior leaves the mapped-speed local at one.

## Caller Context

The sole direct caller is `phy_051_set_xmac_speed @ 0x1c00c` at `0x1c05c`,
where raw PCS mode six delegates runtime speed handling to this helper.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The helper reads two
shared XMAC state arrays and delegates the register write to `xmac_set_speed_sel`.

## Evidence

- Complete 52-instruction ARM64 body at `0x18058` through `0x1812c`.
- Exact byte load of `g_xmac_work_in_auto[xmac]`, work-mode comparison, and
  four stack output-word initializers.
- Exact call/output argument order to
  `xpcs_get_speed_duplex_in_auto_en_sgmii_mode @ 0x19bdc`.
- Complete 61-instruction PCS-reader body confirms it writes speed, duplex, and
  auto-status through `X1`, `X2`, and `X3`, respectively.
- One direct code xref from `phy_051_set_xmac_speed`.
- IDA function type updated at `0x18058`; the corroborating PCS-reader ABI type
  was annotated at `0x19bdc`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware meaning of `g_xmac_work_in_auto` and PCS work mode three.
- Whether `auto_status == 1` denotes link, completion, or another PCS status.
