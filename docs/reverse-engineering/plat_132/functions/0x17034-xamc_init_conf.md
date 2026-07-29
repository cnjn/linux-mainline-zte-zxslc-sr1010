# 0x17034 xamc_init_conf

## Status

- Status: complete
- Confidence: verified selector split, five MMIO operations, fixed literals,
  final RMW, and lack of module callers.
- Size: `0x158` bytes, 86 ARM64 instructions.
- Recovered signature: `void xamc_init_conf(uint8_t xmac)`.

## Semantics

Programs a fixed five-word XMAC configuration. Selector bytes exactly `2` and
`3` use an absolute base `(xmac + 7) << 16` with offsets
`0x0/0x4/0x8/0xa0/0xd00`. Every other byte uses
`nppt_base + (xmac << 18) + 0x140000` with offsets
`0x0/0x10/0x20/0x280/0x3400`.

In order it writes:

1. `0x00010000`
2. `0x3e800086`
3. `0x80000001`
4. `0x00000002`
5. Existing final word OR `0x00000200`

It performs no speed selection, duplex configuration, validation, lock,
barrier, or semantic return operation. It differs from
`xamc_init_conf_by_speed` by omitting that helper's speed/duplex setter calls.

## Caller Context

No direct module callers were found. Runtime kallsyms marks it as local text.

## Evidence

- Complete ARM64 body at `0x17034` through `0x17188`.
- Five selector-dependent address calculations and ordered stores.
- Final read at `0x1714c`, OR at `0x17180`, and store at `0x17184`.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
