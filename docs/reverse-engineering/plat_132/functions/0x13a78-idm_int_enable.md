# 0x13a78 idm_int_enable

## Status

- Status: complete
- Confidence: verified.
- Size: `0x60` bytes, 23 ARM64 instructions.
- Recovered semantic signature: `void idm_int_enable(u32 bits)`.

## Semantics

This helper enables selected IDM interrupt sources by clearing `bits` in the
software mask and writing the result to `nppt_base + 0x280040`:

```c
idm_int_mask &= ~bits;
*(u32 *)(nppt_base + 0x280040) = idm_int_mask;
```

Its paired helper `idm_int_disable @ 0x13ad8` ORs bits into the same software
mask and writes the same MMIO location. Together they establish that a set bit
in `idm_int_mask` denotes a masked IDM source.

## Synchronization

The exact instruction sequence is:

1. Save local IRQ state through `arch_local_irq_save_1`.
2. Call the constant-propagated raw-lock acquisition helper for `idm_lock_int`.
3. Update `idm_int_mask` and issue the MMIO write.
4. Release the lock with a byte-width release store of zero (`STLRB`) to
   `idm_lock_int`.
5. Restore the saved local IRQ state.

The function's meaningful result is not used by any direct caller in this
module. The recovered source models it as `void`; the binary leaves whatever
value the IRQ-restore helper returns in the architectural return register.

## Call Context

- No direct code caller is present; the only xref is the IDM operations table
  at `0x266e0`.
- `idm_int_disable` is called by all four IDM hard IRQ handlers before they
  schedule/process their respective CPU-net work, so later code must re-enable
  sources through this operation-table callback or an equivalent path.

## Evidence

- Full 23-instruction ARM64 disassembly at `0x13a78` through `0x13ad4`.
- Paired decompilation of `idm_int_disable @ 0x13ad8`.
- Decompilation of `idm_cpu_int`, `idm_wifi_int`, `idm_rls_int`, and
  `idm_all_int`, which all call the paired disable helper.
- Raw IDM ops-table bytes at `0x266d0` show pointers to disable at `0x13ad8`
  and enable at `0x13a78`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The exact source-level type/layout of `idm_lock_int` and the operation-table
  callback prototype remain unresolved.
- The later re-enable paths and each IDM mask bit's hardware meaning require
  downstream IRQ and NAPI reconstruction.
