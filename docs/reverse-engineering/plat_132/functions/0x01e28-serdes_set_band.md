# 0x01e28 serdes_set_band

## Status

- Status: complete
- Confidence: verified SerDes offset, both RMW masks, unmasked inputs, log,
  return behavior, and absence of direct xrefs.
- Size: `0x44` bytes, 16 ARM64 instructions.
- Recovered signature: `int serdes_set_band(u32 band_select, u32 band)`.

## Semantics

Performs two sequential read-modify-writes at SerDes offset `0x6c`. The first
clears bit 14 and ORs `band_select << 14`; the second clears bits 0-7 and ORs
`band`. It then returns `printk("set pll band ok\n")`.

Neither input is masked to its apparent field width. A `band_select` above one
bit or a `band` above eight bits can therefore set neighboring register bits.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed SerDes tuning API.

## Evidence

- Complete ARM64 body at `0x1e28` through `0x1e68`.
- Exact offset, masks, shifts, unmasked OR operations, string, and return.
- IDA type at `0x1e28` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
