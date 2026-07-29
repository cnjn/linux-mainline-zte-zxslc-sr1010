# 0x13ad8 idm_int_disable

## Status

- Status: complete
- Confidence: verified.
- Size: `0x60` bytes, 23 ARM64 instructions.
- Recovered semantic signature: `void idm_int_disable(u32 bits)`.

## Semantics

This helper masks selected IDM interrupt sources by ORing `bits` into the
software mask and writing it to `nppt_base + 0x280040`:

```c
idm_int_mask |= bits;
*(u32 *)(nppt_base + 0x280040) = idm_int_mask;
```

`idm_int_enable @ 0x13a78` performs the exact inverse (`&= ~bits`) under the
same synchronization. Therefore a set bit is a masked source.

## Synchronization

The binary saves local IRQ state, calls the constant-propagated raw-lock helper
for `idm_lock_int`, updates software and hardware state, emits a byte-width
release store of zero to the lock (`STLRB`), and restores local IRQ state. The
modeled return type is `void`; no direct caller consumes the architectural
return value left by the restore helper.

## Call Context

All four IDM hard IRQ handlers invoke this helper before `cpu_net_int`:

| Handler | Mask source | `cpu_net_int` argument |
| --- | --- | --- |
| `idm_cpu_int` | `idm_info + 0x0` | 0 |
| `idm_wifi_int` | `idm_info + 0x4` | 1 |
| `idm_rls_int` | `idm_info + 0xc` | 3 |
| `idm_all_int` | `idm_info + 0x8` | 2 |

Each handler returns `1` after that handoff. This establishes a mask-before-
deferred-processing discipline; the corresponding re-enable path remains to be
reconstructed.

## Evidence

- Full 23-instruction ARM64 disassembly at `0x13ad8` through `0x13b34`.
- Paired enable helper at `0x13a78`.
- Direct decompilation and xrefs for the four IDM IRQ handlers at `0x13b38`,
  `0x13b60`, `0x13b88`, and `0x13bb0`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- Which deferred `cpu_net_int` paths re-enable each mask bit is unresolved.
- The exact IDM lock type and operation-table ABI remain unknown.
