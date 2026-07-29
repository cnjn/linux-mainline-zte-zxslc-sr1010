# 0x1309c idm_get_cpu_tx_q

## Status

- Status: complete
- Confidence: verified.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature:
  `struct idm_tx_queue *idm_get_cpu_tx_q(u32 index)`.

## Semantics

This accessor validates the four-element TX queue range, then returns a
40-byte-stride entry:

```c
if (index >= 4)
    return NULL;
return &idm_tx_q[index];
```

The exact ARM64 calculation is `idm_tx_q + 40 * index` after the `index <= 3`
test. This corroborates the four consecutive 40-byte TX queue entries initialized
by `idm_init`.

## Call Context

- It has no direct code callers in this module.
- The only xref is an IDM ops-table entry at `0x266f8`, making the null return
  the only in-module guard against invalid external TX queue indices.

## Evidence

- Full 10-instruction ARM64 disassembly at `0x1309c` through `0x130c0`.
- Raw IDM ops-table data at `0x266d8`.
- TX descriptor initialization in `idm_init @ 0x14ff4` for four 40-byte entries.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The original source-level type name and meanings of TX queue fields remain
  inferred from the initializer.
