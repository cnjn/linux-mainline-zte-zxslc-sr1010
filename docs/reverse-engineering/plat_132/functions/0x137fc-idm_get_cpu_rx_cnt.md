# 0x137fc idm_get_cpu_rx_cnt

## Status

- Status: complete
- Confidence: verified for register arithmetic and bit assembly; register-field
  meaning unknown.
- Size: `0x68` bytes, 26 ARM64 instructions.
- Recovered signature: `u32 idm_get_cpu_rx_cnt(u32 index)`.

## Semantics

For indices zero through seven, the function reads two 32-bit words and
reconstructs a 32-bit result from matching 16-bit halves:

```c
group = index >> 1;
first  = *(u32 *)(nppt_base + 0x280000 + 4 * (group + 0x31));
second = *(u32 *)(nppt_base + 0x280000 + 4 * (group + 0x35));

if (index & 1)
    return (second & 0xffff0000) | (first >> 16);
return (second << 16) | (first & 0xffff);
```

For any index greater than seven, it returns the raw 32-bit word at:

```c
nppt_base + 0x280000 + 4 * ((index + 0x31) & 0x3fffffff)
```

There is no range check in either path. The two-word path has no lock, latch, or
retry, so hardware changes between the reads can produce a non-atomic snapshot.

## Call Context

The only xref is an IDM ops-table entry at `0x266e8`; no direct in-module
caller constrains valid indices or assigns register-field names.

## Evidence

- Full 26-instruction ARM64 disassembly at `0x137fc` through `0x13860`.
- Raw IDM ops-table data at `0x266d8`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The mapping from index to IDM counter class and the hardware names of the
  split 16-bit fields are unknown.
- Consumers must define acceptable behavior for non-atomic counter snapshots.
