# 0x18bcc xpcs_wait_vr_xs_pcs_dig_sts_pseq_state.constprop.0

## Status

- Status: complete
- Confidence: verified register selection, PSEQ-state condition, retry bound,
  delay, returns, and both direct callers.
- Size: `0x9c` bytes, 39 ARM64 instructions.
- Recovered signature:
  `int xpcs_wait_vr_xs_pcs_dig_sts_pseq_state_constprop_0(u8 xmac)`.

## Semantics

The helper polls selector-specific PCS offset `0x0e0040` up to 400 times. It
returns zero when bits 2 through 4 are not equal to four. While they equal four,
it delays with `__const_udelay(859000)` and retries; after the final retry it
returns `-1`.

Selectors two and three use the raw `0x0e0040 + (xmac << 23)` window. Other
selectors use `xmac0_pcs_base + 0x0e0040 + sign_extend32(xmac << 24)`.

## Caller Context

`xpcs_1g_mode_conf @ 0x1952c` and `xpcs_hsgmii_mode_conf @ 0x19fe8` use this
as their PSEQ transition wait. Both treat a nonzero result as configuration
failure.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The poll occupies
the calling context for up to 400 delays.

## Evidence

- Complete ARM64 body at `0x18bcc` through `0x18c64`.
- Exact `((status >> 2) & 7) != 4` exit condition.
- Retry count 400 and `__const_udelay(859000)` call.
- Exhaustive direct xref query found the two configuration callers.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- The physical PSEQ state represented by value four.
