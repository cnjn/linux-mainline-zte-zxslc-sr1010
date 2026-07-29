# 0x19184 xpcs_sr_mii_ctrl_is_an_enable

## Status

- Status: complete
- Confidence: verified selector/address branches, bit-12 extraction,
  normalized unsigned return, and sole direct caller.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature: `u32 xpcs_sr_mii_ctrl_is_an_enable(u8 xmac)`.

## Semantics

The function truncates its selector to a byte, selects the same SR-MII control
word used by the adjacent speed, duplex, and AN writers, reads it once, and
returns bit 12 normalized to zero or one.

Selectors two and three use the raw `0x7c0000 + (xmac << 23)` window. Other
selectors use the established `xmac0_pcs_base`-relative address formula.

## Caller Context

The sole direct caller is `xmac_set_pcs_for_sgmii_half_duplex @ 0x1874c` at
`0x187ac`. In that helper, a nonzero result prevents the default PCS speed,
duplex, link-status, and AN-enable writes.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It is a standalone
volatile read and can observe concurrent register changes.

## Evidence

- Complete 17-instruction ARM64 body at `0x19184` through `0x191c4`.
- Exact byte truncation, special selector test, both address paths, and
  `UBFX X0,X0,#12,#1` return extraction.
- Exhaustive direct xref query found only the call at `0x187ac`.
- IDA type at `0x19184` updated to the recovered byte-selector/unsigned-result
  signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact physical semantics of bit 12 beyond the vendor AN-enable label.
