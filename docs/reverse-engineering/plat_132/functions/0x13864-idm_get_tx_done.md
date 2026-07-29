# 0x13864 idm_get_tx_done

## Status

- Status: complete
- Confidence: verified for register arithmetic and return truncation; field
  meaning unknown.
- Size: `0x34` bytes, 13 ARM64 instructions.
- Recovered signature: `u16 idm_get_tx_done(u32 index)`.

## Semantics

Index zero uses a dedicated register at `nppt_base + 0x280084`. Every nonzero
index uses a computed register offset. Both paths truncate to the low 16 bits:

```c
if (index == 0)
    value = *(u32 *)(nppt_base + 0x280084);
else
    value = *(u32 *)(nppt_base + 0x280000 +
                     4 * ((index + 0x2a) & 0x3fffffff));
return (u16)value;
```

There is no bounds check and no synchronization around the MMIO read.

## Call Context

This function is exposed through the IDM ops table and has no direct in-module
code caller. The name is an IDA label; the exact hardware field behind the low
16-bit result is not yet established.

## Evidence

- Full 13-instruction ARM64 disassembly at `0x13864` through `0x13894`.
- Raw IDM ops-table data at `0x266d8`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The valid index range and precise completion-counter semantics need hardware
  or downstream TX-path evidence.
