# 0x0410c serdes_set_tx_eq

## Status

- Status: complete
- Confidence: verified both accepted values, invalid-value no-op path, exact
  read-modify-write mask/value pairs, log strings, constant return, and export
  context.
- Size: `0x70` bytes, 26 ARM64 instructions.
- Recovered signature: `int serdes_set_tx_eq(uint32_t tx_eq)`.

## Semantics

This exported setter recognizes only two exact input values:

| `tx_eq` | MMIO operation at `pon_serdes_base + 0x20` | Log |
| --- | --- | --- |
| 0 | Replace bits `15:8` with `0x0d` | `set tx 3db pre and post success` |
| 1 | Replace bits `15:8` with `0x1d` | `set tx 6db pre and post success` |
| any other value | No MMIO read, write, or log | none |

For each accepted value, the machine performs one volatile 32-bit load, masks
it with `0xffff00ff`, ORs the selected eight-bit field, and writes the result
back. Bits `31:16` and `7:0` are preserved. The leading and trailing newline
characters in both vendor log strings are retained in the reconstruction.

## Return Semantics

Both successful paths call `printk`, but converge at `0x4170`, where
`MOV W0, #0` discards the logging result. Invalid input branches directly to
the same instruction. The function therefore always returns 0.

## Caller Context

There are no direct code or data xrefs to this entry in the current IDB. It is
nevertheless an externally visible module API: the vendor runtime's
`system/proc/kallsyms` contains both `__ksymtab_serdes_set_tx_eq` and global
text symbol `serdes_set_tx_eq [plat_132]`.

## Evidence

- Full body at `0x410c` through `0x4178`.
- `CBZ W0, 0x4148`, `CMP W0, #1`, and `B.NE 0x4170` establish the exact input
  domain and no-op path.
- Value-1 RMW at `0x4128`-`0x4138`: `AND #0xffff00ff`, `ORR #0x1d00`, `STR`.
- Value-0 RMW at `0x4150`-`0x4160`: `AND #0xffff00ff`, `ORR #0x0d00`, `STR`.
- Shared `printk` call at `0x416c` and constant-zero epilogue at `0x4170`.
- IDA type at `0x410c` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
