# 0x19ba4 xpcs_set_speed_duplex_in_sgmii_anto_disale_mode

## Status

- Status: complete
- Confidence: verified selector narrowing, three argument ABI, call order,
  fixed link-status write, void return, and sole direct caller.
- Size: `0x38` bytes, 14 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_speed_duplex_in_sgmii_anto_disale_mode(u8 xmac, u32 speed, u32 state)`.

## Semantics

The helper first truncates `xmac` to a byte and saves `state` from `W2` in
`W7`. It then executes this fixed sequence:

1. `xpcs_set_sr_mii_ctrl_speed(xmac, speed)`.
2. `xpcs_set_sr_mii_ctrl_duplex_mode(xmac, state)`.
3. `xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(xmac, 1)`.

It does not read or branch on the return values of its callees. The final
callee's pointer return register is incidental and is not used by the caller,
so the reconstructed interface is `void`.

## Caller Context

The sole direct caller is
`xmac_set_pcs_for_sgmii_half_duplex @ 0x1874c` at `0x1879c`, in its
`configure == 1` branch. That caller preserves the original fourth input in
`W2` before this call, proving the wrapper receives the three argument values
in `(xmac, speed, state)` order.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. Its direct register
effects occur in the three delegated PCS helpers.

## Evidence

- Complete 14-instruction ARM64 body at `0x19ba4` through `0x19bd8`.
- `UXTB W6,W0` proves byte selector narrowing; `MOV W7,W2` preserves the third
  argument across the first callee.
- Call order and fixed `MOV W1,#1` before the link-status setter.
- Exhaustive direct xref query found only `0x1879c` in `0x1874c`.
- IDA type at `0x19ba4` updated to the recovered void three-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Vendor spelling `anto_disale` may be a typo and is preserved in the recovered
  symbol.
- Exact hardware meaning of the state input is determined by the lower PCS
  duplex writer rather than this wrapper.
