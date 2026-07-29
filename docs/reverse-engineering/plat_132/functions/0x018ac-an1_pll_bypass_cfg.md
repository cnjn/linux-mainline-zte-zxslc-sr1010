# 0x018ac an1_pll_bypass_cfg

## Status

- Status: complete
- Confidence: verified range log, seven snapshots, all mode branches, every
  PLL/SerDes RMW, void ABI, and absence of direct xrefs.
- Size: `0x18c` bytes, 97 ARM64 instructions.
- Recovered signature: `void an1_pll_bypass_cfg(u32 bypass_mode)`.

## Semantics

Snapshots PLL offsets `0xc/0x10` and SerDes offsets `0xc/0x10/0x54/0x64/0x74`
on every call. Mode zero writes those just-read snapshots back. Modes one and
two enable bypass and program distinct SerDes offset-`0xc` fields, followed by
a shared sequence at offsets `0x54`, `0x64`, and `0x74`. Values above two log a
range message, still take snapshots, then log an error without register setup.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL diagnostic API.

## Evidence

- Complete ARM64 body at `0x18ac` through `0x1a34`.
- Exact snapshots, mode comparisons, masks/constants, write order, and strings.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x18ac` updated to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
