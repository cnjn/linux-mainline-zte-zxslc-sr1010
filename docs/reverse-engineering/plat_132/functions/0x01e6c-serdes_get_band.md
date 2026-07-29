# 0x01e6c serdes_get_band

## Status

- Status: complete
- Confidence: verified SerDes offset, extracted field, log, unsigned return,
  and absence of direct xrefs.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `u32 serdes_get_band(void)`.

## Semantics

Extracts bits 16-23 from SerDes offset `0xd0`, logs the resulting byte, and
returns it as an unsigned value.

Despite its name, this getter does not read the `0x6c` register written by
`serdes_set_band @ 0x1e28`. The offset mismatch is direct binary evidence and
is preserved rather than normalized into an assumed setter/getter pair.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed SerDes status API.

## Evidence

- Complete ARM64 body at `0x1e6c` through `0x1ea4`.
- `LDR W19, [X0,#0xD0]` followed by `UBFX X19, X19, #0x10, #8`.
- Exact log string and restored return after `printk`.
- IDA type at `0x1e6c` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
