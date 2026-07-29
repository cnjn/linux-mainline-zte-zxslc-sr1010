# 0x1710 register_nppt_int

## Status

- Status: complete
- Confidence: verified
- Size: `0x70` bytes, 25 ARM64 instructions.

## Recovered Signature

```c
int register_nppt_int(void);
```

## Semantics

This is structurally identical to `register_pon_int`, with this argument tuple:

| Argument | Observed value |
| --- | --- |
| IRQ | `g_nppt_irq` |
| primary handler | `zx_nppt_int` |
| threaded handler | null |
| flags | 0 |
| name | `"nppt"` |
| dev-id | `&pon_int_info` |

It returns a negative `request_threaded_irq` result unchanged and returns zero
for a nonnegative result. Its paired removal uses
`free_irq(g_nppt_irq, &pon_int_info)`.

## Pairing and Runtime Evidence

- `zx_pon_probe` calls this only after `register_pon_int` succeeds.
- `zx_pon_remove` releases it after releasing PON IRQ.
- The vendor runtime interrupt list shows IRQ 25 named `nppt`.

## Evidence

- `0x1744`: load `g_nppt_irq`.
- `0x173c`/`0x174c`: construct `zx_nppt_int` handler.
- `0x1750`/`0x1754`: null thread handler and zero flags.
- `0x1738`/`0x1748`: name `"nppt"`.
- `0x1730`/`0x1740`: `&pon_int_info` dev-id.
- `0x175c`: sign-bit-only error check.

## Source-Like Reconstruction

The reconstructed function is appended to
`docs/reverse-engineering/plat_132/recovered/plat_irq.c`.

## Open Questions

- Reconstruct `zx_nppt_int` to document the OAM/PTP/PON event dispatch.
- Determine the intended ownership model of shared `pon_int_info` across all
  vendor interrupt registration helpers.
