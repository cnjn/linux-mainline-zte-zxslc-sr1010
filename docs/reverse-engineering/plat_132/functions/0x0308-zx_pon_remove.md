# 0x0308 zx_pon_remove

## Status

- Status: complete
- Confidence: verified
- Size: `0x1c` bytes, 7 ARM64 instructions.

## Recovered Signature

```c
int zx_pon_remove(struct platform_device *pdev);
```

The platform-device argument is unused. Its candidate type is retained only to
match the platform-driver callback ABI; no upstream source was used as vendor
implementation evidence.

## Semantics

1. Call `unregister_pon_int`.
2. Call `unregister_nppt_int`.
3. Return zero unconditionally.

`unregister_pon_int` calls `free_irq(g_pon_irq, &pon_int_info)`, and
`unregister_nppt_int` calls `free_irq(g_nppt_irq, &pon_int_info)`. These exact
IRQ/dev-id pairs match the successful registration helpers used by probe.

## Cleanup Boundary

This callback directly releases only the PON and NPPT top-level IRQs. It does
not directly release:

- IDM IRQs 26 through 29.
- PPS or WOE IRQ state.
- OF mappings or manual low-power ioremaps.
- SMAC, IDM, NAPI, or PON global state.

That is not proof of a full module-exit leak: `plat_cleanupModule` invokes
`nppt_exit` before platform-driver unregister invokes this callback. Those
separate cleanup functions must be reconstructed before assigning ownership.

## Evidence

- `0x0310`: direct call to `unregister_pon_int`.
- `0x0314`: direct call to `unregister_nppt_int`.
- `0x0318`: `MOV W0, #0` establishes the unconditional result.
- Decompilation of both unregister helpers verifies their `free_irq` targets
  and common `&pon_int_info` dev-id argument.

## Source-Like Reconstruction

The reconstructed function is appended to
`docs/reverse-engineering/plat_132/recovered/plat_probe.c`.

## Open Questions

- Attribute IDM and mapping cleanup to the responsible exit functions after
  reconstructing `nppt_exit` and the IDM teardown routines.
