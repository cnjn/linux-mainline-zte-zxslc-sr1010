# 0x19cd4 xpcs_exit_sgmii_mode

## Status

- Status: complete
- Confidence: verified byte selector, fixed clear sequence, void return, and
  sole direct caller.
- Size: `0x5c` bytes, 23 ARM64 instructions.
- Recovered signature: `void xpcs_exit_sgmii_mode(u8 xmac)`.

## Semantics

The helper truncates its selector to a byte and clears these PCS controls in
the exact order below:

1. SR-MII AN enable.
2. VR-MII AN interrupt enable.
3. VR-MII MAC-auto-switch.
4. VR-XS/PCS digital-control 2.5G mode enable.
5. VR-MII digital-control 2.5G mode enable.
6. VR-MII AN-control SGMII link status.

Every callee receives literal zero as the second argument.

## Caller Context

`xpcs_prepare_for_switch_mode @ 0x19d5c` is the sole direct caller. It selects
this exit path only when cached `sg_xpcs_mode[xmac]` equals three and the target
mode differs.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It relies on the
lower PCS control helpers for all volatile writes.

## Evidence

- Complete 23-instruction ARM64 body at `0x19cd4` through `0x19d2c`.
- Repeated selector preservation in `W5` and six literal-zero second arguments.
- Exhaustive direct xref query found only the dispatch call at `0x19dc0`.
- IDA type at `0x19cd4` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS hardware roles of the two vendor-named 2.5G mode controls.
