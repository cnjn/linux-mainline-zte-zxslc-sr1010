# 0x18530 xmac_speed_process_in_usxgmii_auto_mode

## Status

- Status: complete
- Confidence: verified all four gates, exact PCS output-pointer order, default
  output values, speed conversion/write, logging, void return, and no external
  xrefs.
- Size: `0xdc` bytes, 53 ARM64 instructions.
- Recovered signature:
  `void xmac_speed_process_in_usxgmii_auto_mode(u8 xmac)`.

## Semantics

The helper initializes local `uni_speed`, `duplex`, and `auto_status` words to
zero and initializes the mapped XMAC-speed word to one. It returns without a
side effect unless all conditions hold:

1. `g_xmac_work_in_auto[xmac]` is nonzero.
2. `sg_xmac_work_mode[xmac]` is one of `5`, `6`, or `7`, evaluated through the
   raw unsigned `(work_mode - 5) <= 2` test.
3. `xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode(xmac, &uni_speed, &duplex,
   &auto_status)` returns zero.
4. `auto_status == 1`.

On success it maps `uni_speed` through
`xmac_switch_uni_speed_to_xmac_speed`, writes the result through
`xmac_set_speed_sel`, and logs both speed values. The `duplex` output is passed
to the PCS helper but otherwise unused here. An unsupported converted speed
leaves the initialized mapped-speed word at one.

## Caller Context

No in-module code or data reference targets this function in the current IDB.
It is retained as an unreferenced local USXGMII helper, not assigned a
speculative caller or callback-table role.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The helper reads two
shared XMAC state arrays and delegates the register write to `xmac_set_speed_sel`.

## Evidence

- Complete 53-instruction ARM64 body at `0x18530` through `0x18608`.
- Exact byte load of `g_xmac_work_in_auto[xmac]` and unsigned mode-five-through-
  seven range check.
- Exact call/output argument order to
  `xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode @ 0x19910`.
- Complete PCS-reader body confirms it writes speed, duplex, and auto-status
  through `X1`, `X2`, and `X3`, respectively.
- Full xref query shows no external code or data reference.
- IDA function type updated at `0x18530`; the corroborating PCS-reader ABI type
  was annotated at `0x19910`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Why this local helper is unreferenced in the analyzed module image.
- Exact hardware meaning of auto flag, work modes five through seven, and
  `auto_status == 1`.
