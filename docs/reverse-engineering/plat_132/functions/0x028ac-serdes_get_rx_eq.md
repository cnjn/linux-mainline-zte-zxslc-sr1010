# 0x028ac serdes_get_rx_eq

## Status

- Status: complete
- Confidence: verified single MMIO read, all active-low enables and fields,
  exact log strings, return behavior, and absence of direct xrefs.
- Size: `0xc4` bytes, 43 ARM64 instructions.
- Recovered signature: `int serdes_get_rx_eq(void)`.

## Semantics

Takes one snapshot of SerDes offset `0x2c` and reports four receive-equalizer
fields:

| Item | Enable state | Data field |
| --- | --- | --- |
| EQ1 | bit 0, active low | bits 3-7 |
| EQ2 | bit 1, active low | bits 8-12 |
| EQ3 | bit 2, active low | bits 13-17 |
| MBF | always reported | bits 18-21 |

For each enabled EQ, it prints the enable message and its five-bit data value;
for a disabled EQ, it prints only the disable message. The enable/disable
strings contain no newline. The function always prints MBF last and returns
that final `printk` result rather than any register field.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed diagnostic API.

## Evidence

- Complete ARM64 body at `0x28ac` through `0x296c`.
- One `LDR W19, [X0,#0x2C]` and three `TBNZ` active-low branches.
- Exact `UBFX` fields `(3,5)`, `(8,5)`, `(13,5)`, and `(18,4)`.
- Exact strings and final tail return from the MBF `printk`.
- IDA type at `0x28ac` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
