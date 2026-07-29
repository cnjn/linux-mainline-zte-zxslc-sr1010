# 0x198b4 xpcs_switch_vr_mii_an_intr_sts_speed

## Status

- Status: complete
- Confidence: verified all computed-dispatch targets, output write, unsigned
  return, and all direct callers.
- Size: `0x5c` bytes, 22 ARM64 instructions.
- Recovered signature:
  `u32 xpcs_switch_vr_mii_an_intr_sts_speed(u32 reported_speed, u32 *uni_speed)`.

## Semantics

The helper maps an unsigned raw AN status speed code to a UNI speed code, stores
that code through `uni_speed`, and returns the same code:

| Raw code | UNI code |
| --- | --- |
| 0 | 1 |
| 1 | 2 |
| 2 | 3 |
| 3 | 6 |
| 4 | 4 |
| 5 | 5 |
| Any other unsigned value | 7 |

## Caller Context

The USXGMII and SGMII auto-enable speed/duplex handlers use this mapping after
capturing their AN interrupt status speed field. Their callers use the output
value for SR-MII programming and caller-owned speed output storage.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. Output storage is
owned by the caller.

## Evidence

- Complete 22-instruction ARM64 body at `0x198b4` through `0x1990c`.
- Six computed jump-table targets and default target at `0x19904`.
- Final store and return share the same mapped `W0` value.
- Exhaustive direct xref query found two caller sites.
- IDA type at `0x198b4` updated to the recovered unsigned return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact physical speed meanings of the raw AN and UNI codebooks.
