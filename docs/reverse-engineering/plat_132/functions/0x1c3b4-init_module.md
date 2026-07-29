# 0x1c3b4 init_module

## Status

- Status: complete
- Confidence: verified
- Size: `0x34` bytes, 12 ARM64 instructions, `.init.text`.
- IDA names: `init_module`; alternative name `plat_initModule`.

## Recovered Signature

```c
int plat_initModule(void);
```

`int` is strongly supported by the use of `W0` for the conditional return
status. The init-section placement supports, but does not by itself prove, the
original use of a kernel `__init` annotation or `module_init` macro.

## Semantics

1. Store `2` in `g_pon_cputype`.
2. Print a message identifying that value as `ZTE_PON_CPUTYPE_133`.
3. Register the PON platform driver through `pon_driver_register`.
4. Return the registration failure unchanged.
5. Only after successful registration, invoke and return the result of
   `nppt_init`.

The store is independently verified by the helper functions:

- `isCpuType_133`: `g_pon_cputype == 2`.
- `isCpuType_132`: `g_pon_cputype == 1`.
- `isCpuType_129`: `g_pon_cputype == 4`.

## Source-Like Reconstruction

The reconstructed C is in
`docs/reverse-engineering/plat_132/recovered/plat_module.c`.

## Evidence

- `0x1c3c4`: `STR W1, [g_pon_cputype]`, after `MOV W1, #2`.
- `0x1c3d0`: direct call to `printk` with the 133 CPU-type message.
- `0x1c3d4`: direct call to `pon_driver_register`.
- `0x1c3d8`: `CBNZ W0, 0x1c3e0`; nonzero registration status bypasses NPPT.
- `0x1c3dc`: direct call to `nppt_init` only on zero status.
- `pon_driver_register @ 0x0e8c` directly calls
  `__platform_driver_register(zx_pon_driver, &__this_module)`.

## Error and Ownership Notes

- The return value of `printk` remains in `X0` at the next call under the ARM64
  calling convention, but `pon_driver_register` has no semantic argument and
  does not consume it. This is compiler register reuse, not a data dependency.
- If `nppt_init` fails after platform-driver registration succeeds, this
  function returns the failure without unregistering the platform driver.
  Preserve that behavior in the reconstruction; cleanup ownership belongs to a
  later lifecycle analysis.
- No upstream source tree was used as implementation evidence.

## Open Questions

- Recover the module-exit path and platform-driver data before deciding whether
  the missing `nppt_init` failure unwind is intentional or a vendor defect.
