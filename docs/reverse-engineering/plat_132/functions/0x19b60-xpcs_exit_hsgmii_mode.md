# 0x19b60 xpcs_exit_hsgmii_mode

## Status

- Status: complete
- Confidence: verified byte selector, fixed clear sequence, void return, and
  sole direct caller.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature: `void xpcs_exit_hsgmii_mode(u8 xmac)`.

## Semantics

The helper truncates its selector to a byte and clears these controls in the
exact order below:

1. VR-MII digital-control 2.5G mode enable.
2. VR-MII digital-control VSMMD1 enable.
3. VR-XS/PCS digital-control 2.5G mode enable.
4. VR-XS/PCS digital-control VSMMD1 enable.

Every callee receives literal zero as the second argument.

## Caller Context

`xpcs_prepare_for_switch_mode @ 0x19d5c` is the sole direct caller. It selects
this helper only when cached `sg_xpcs_mode[xmac]` equals eight and the target
mode differs.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It relies on lower
PCS control helpers for all volatile writes.

## Evidence

- Complete 17-instruction ARM64 body at `0x19b60` through `0x19ba0`.
- Repeated selector preservation in `W5` and four literal-zero second arguments.
- Exhaustive direct xref query found only the dispatch call at `0x19db8`.
- IDA type at `0x19b60` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS hardware behavior of the paired MII and XS/PCS VSMMD1 controls.
