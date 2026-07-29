# 0x16a0 register_pon_int

## Status

- Status: complete
- Confidence: verified
- Size: `0x70` bytes, 25 ARM64 instructions.

## Recovered Signature

```c
int register_pon_int(void);
```

## Semantics

The function invokes the vendor kernel's `request_threaded_irq` import with:

| Argument | Observed value |
| --- | --- |
| IRQ | `g_pon_irq` |
| primary handler | `zx_pon_int` |
| threaded handler | null |
| flags | 0 |
| name | `"pon"` |
| dev-id | `&pon_int_info` |

It returns the request status only when its sign bit is set. Otherwise it
returns zero. Therefore a normal zero return is preserved, while a hypothetical
positive return would also be normalized to zero.

## Pairing and Runtime Evidence

- `unregister_pon_int` releases exactly
  `free_irq(g_pon_irq, &pon_int_info)`.
- `zx_pon_probe` calls this helper before `register_nppt_int`.
- The runtime interrupt list shows IRQ 21 with the name `pon`, matching the
  requested IRQ label.

## Evidence

- `0x16d4`: load `g_pon_irq` into the first argument register.
- `0x16cc` and `0x16dc`: construct `zx_pon_int` handler address.
- `0x16e0` and `0x16e4`: set thread handler and flags to zero.
- `0x16c8`/`0x16d8`: construct name `"pon"`.
- `0x16c0`/`0x16d0`: construct `&pon_int_info`.
- `0x16ec`: test only bit 31 of the return value.

The conventional argument labels for `request_threaded_irq` are ABI vocabulary
only. The actual values and control flow above are directly verified in the
vendor binary.

## Source-Like Reconstruction

The reconstructed function is in
`docs/reverse-engineering/plat_132/recovered/plat_irq.c`.

## Open Questions

- Reconstruct `zx_pon_int` to determine all PON interrupt status bits and
  callback dispatch behavior.
- Determine why multiple vendor interrupt registration helpers share
  `pon_int_info` as the dev-id.
