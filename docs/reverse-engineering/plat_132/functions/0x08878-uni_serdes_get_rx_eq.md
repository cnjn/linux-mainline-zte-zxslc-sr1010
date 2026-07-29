# 0x08878 uni_serdes_get_rx_eq

## Status

- Status: complete
- Confidence: verified one register snapshot, all enable/data branches, MBF
  extraction, final log return, absent internal xrefs, and exported ABI.
- Size: `0xc4` bytes, 43 ARM64 instructions.
- Recovered signature: `int uni_serdes_get_rx_eq(void)`.

## Semantics

Takes one snapshot of `uni_serdes_base + 0x2c` and emits diagnostics:

| Field | Disable bit | Value when enabled |
| --- | --- | --- |
| RX EQ1 | bit 0 | bits 3-7 |
| RX EQ2 | bit 1 | bits 8-12 |
| RX EQ3 | bit 2 | bits 13-17 |
| MBF | none | bits 18-21, always logged |

A set disable bit emits only that EQ's disabled message; a clear bit emits the
enabled message and its five-bit value. The function returns the final MBF
`printk` result. It is exported and has no internal IDB xrefs.

## Evidence

- Complete ARM64 body at `0x8878` through `0x8938`.
- Single volatile snapshot at `0x888c`.
- EQ1, EQ2, and EQ3 branch/value groups at `0x8890`-`0x88bc`,
  `0x88c0`-`0x88ec`, and `0x88f0`-`0x891c`.
- MBF extraction `UBFX #18,#4` at `0x8924`; final returned log call at
  `0x892c`.
- IDA type at `0x8878` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
