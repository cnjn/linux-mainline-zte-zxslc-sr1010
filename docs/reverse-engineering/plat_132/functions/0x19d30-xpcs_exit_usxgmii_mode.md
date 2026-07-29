# 0x19d30 xpcs_exit_usxgmii_mode

## Status

- Status: complete
- Confidence: verified byte selector, fixed two-call sequence, void return, and
  sole direct caller.
- Size: `0x2c` bytes, 11 ARM64 instructions.
- Recovered signature: `void xpcs_exit_usxgmii_mode(u8 xmac)`.

## Semantics

The helper truncates its selector to a byte, then invokes this exact sequence:

1. `xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en(xmac, 0)`.
2. `xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en(xmac, 1)`.

It does not inspect return values from either lower PCS writer.

## Caller Context

`xpcs_prepare_for_switch_mode @ 0x19d5c` is the sole direct caller. It selects
this helper for cached modes five through seven when the requested target differs.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The lower helpers
perform all PCS writes.

## Evidence

- Complete 11-instruction ARM64 body at `0x19d30` through `0x19d58`.
- Preserved byte selector and literal zero/one second arguments.
- Exhaustive direct xref query found only the dispatch call at `0x19db0`.
- IDA type at `0x19d30` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS hardware relationship between the USXGMII and VSMMD1 controls.
