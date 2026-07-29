# 0x0e8c pon_driver_register

## Status

- Status: complete
- Confidence: verified for call behavior; strong inference for platform-driver
  field labels.
- Size: `0x28` bytes, 10 ARM64 instructions.

## Recovered Signature

```c
int pon_driver_register(void);
```

## Semantics

Pass the statically allocated `zx_pon_driver` object and module owner object to
`__platform_driver_register`, then return its status unchanged. No memory is
allocated, no state is changed locally, and no error translation occurs.

## Binary Evidence

- `0x0e90` through `0x0ea4` form `X0 = 0x26428 + 0x10 = 0x26438`, the address
  named `zx_pon_driver` by IDA.
- `0x0e94` through `0x0ea0` form `X1 = &__this_module` at `0x27340`.
- `0x0ea8` directly calls `__platform_driver_register`.
- `0x0eac` through `0x0eb0` return its unchanged `W0` status.
- `pon_driver_unregister @ 0x0eb4` directly invokes
  `platform_driver_unregister(zx_pon_driver)`.
- `plat_cleanupModule @ 0x1c3e8` calls `nppt_exit` before
  `pon_driver_unregister`, establishing module-exit ordering.

## Driver Object Facts

Direct data values and strings establish these fields of `zx_pon_driver`:

| Object offset | Observed value | Interpretation | Confidence |
| --- | --- | --- | --- |
| `+0x00` | `0x0580` | `zx_pon_probe` callback | verified |
| `+0x08` | `0x0308` | `zx_pon_remove` callback | verified |
| `+0x28` | `0x1ead2` | driver name `zte,zx279133-pon` | verified |
| `+0x38` | `0x27340` | module owner `__this_module` | verified |
| `+0x50` | `0x1d218` | `zx_pon_match` reference | verified |

The labels `probe`, `remove`, `driver.name`, `driver.owner`, and
`driver.of_match_table` are compatible with the upstream Linux 5.4.196
`struct platform_driver` ABI. That upstream header was used only as an ABI
reference; the addresses and data relationships above come from the vendor
binary.

## Source-Like Reconstruction

The reconstructed function is appended to
`docs/reverse-engineering/plat_132/recovered/plat_module.c`.

## Open Questions

- Recover the complete `zx_pon_driver` object and `zx_pon_match` table before
  emitting a typed static driver initializer.
- Determine whether the vendor platform-driver structure diverges beyond the
  observed fields.
