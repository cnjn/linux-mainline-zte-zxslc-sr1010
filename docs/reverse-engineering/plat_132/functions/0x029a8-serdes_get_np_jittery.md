# 0x029a8 serdes_get_np_jittery

## Status

- Status: complete
- Confidence: verified single MMIO read, SerDes field, log, unsigned return,
  and absence of direct xrefs.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `u32 serdes_get_np_jittery(void)`.

## Semantics

Takes one snapshot of SerDes offset `0x48`, extracts bits 6-8, logs the
three-bit value, and returns it unsigned.

Hex-Rays renders the logging argument as another memory expression, but the
assembly contains only one `LDR` from offset `0x48`; both the log and return
use the extracted value retained in `w19`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed SerDes diagnostic API.

## Evidence

- Complete ARM64 body at `0x29a8` through `0x29e0`.
- One `LDR W19, [X0,#0x48]` followed by `UBFX X19, X19, #6, #3`.
- Exact log string and restored unsigned return after `printk`.
- IDA type at `0x29a8` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
