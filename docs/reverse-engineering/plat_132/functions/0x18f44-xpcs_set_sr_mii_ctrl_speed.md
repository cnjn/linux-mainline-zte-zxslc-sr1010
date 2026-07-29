# 0x18f44 xpcs_set_sr_mii_ctrl_speed

## Status

- Status: complete
- Confidence: verified selector narrowing/address branches, read-before-switch
  order, all speed encodings, unsupported-input behavior, void return, and all
  direct callers.
- Size: `0xcc` bytes, 50 ARM64 instructions.
- Recovered signature: `void xpcs_set_sr_mii_ctrl_speed(u8 xmac, u32 speed)`.

## Semantics

The function narrows the selector to a byte and selects a PCS SR-MII control
word:

- Selectors two and three use raw addresses
  `0x7c0000 + (xmac << 23)`.
- Other selectors use `xmac0_pcs_base + 0x7c0000 + sign_extend32(xmac << 24)`.

It reads that volatile word before validating the speed code. For speed values
one through six, it clears bits 13, 6, and 5 with `0xffffdf9f`, then ORs the
following replacement value:

| Speed input | Replacement bits |
| --- | --- |
| 1 | `0x0000` |
| 2 | `0x2000` |
| 3 | `0x0040` |
| 4 | `0x0020` |
| 5 | `0x2020` |
| 6 | `0x2040` |

Every other input returns after the volatile read without a write.

## Caller Context

Six direct callers use this writer: the recovered SGMII half-duplex path and
wrapper, USXGMII auto-disable configuration, 1G mode configuration, USXGMII
return value.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 50-instruction ARM64 body at `0x18f44` through `0x1900c`.
- Selector tests and exact direct/base-relative pointer arithmetic.
- Initial `LDR W5,[X2]` occurs before the speed range test.
- Complete six-target computed jump table and exact mask at `0x18fd4`.
- Exhaustive direct xref query found six caller sites.
- IDA type at `0x18f44` updated to the recovered void two-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS field names and physical speed meanings for the six vendor codes.
- Why selectors two and three use a raw window while the other path adds the
  mapped `xmac0_pcs_base` pointer.
