# 0x01fc8 serdes_set_error_time

## Status

- Status: complete
- Confidence: verified mode ranges, constants, 32-bit multiplication, register
  writes, logged readback, caller, and return behavior.
- Size: `0x80` bytes, 29 ARM64 instructions.
- Recovered signature: `int serdes_set_error_time(u32 seconds)`.

## Semantics

Computes a 32-bit measurement duration from `seconds` and `pon_serdes_mode`:

| SerDes mode | 32-bit value written to offset `0x98` |
| --- | --- |
| 0-4 | `seconds * 156250000` |
| 5-7 | `seconds * 155520000` |
| all other unsigned values | `0` |

The ARM64 `MUL W` operations retain only the low 32 bits. After writing that
word, the function clears bits 24-31 at offset `0xa4`, reads both registers
back as an apparent 40-bit value, logs it, and returns the `printk` result.
Because the high byte is explicitly cleared, the programmed/logged duration
is truncated to 32 bits.

## Caller Context

`serdes_get_hard_prbs_cnt @ 0x43ec` is the only direct caller. It passes its
unsigned time argument before configuring PRBS receive mode and enabling the
checker, error counter, and measurement timer.

## Evidence

- Complete ARM64 body at `0x1fc8` through `0x2044`.
- Unsigned mode comparisons covering 0-4 and 5-7.
- Constants `0x09502f90` (156250000) and `0x09450c00` (155520000), each used
  by a 32-bit `MUL W`.
- Writes at offsets `0x98` and `0xa4`, followed by exact 40-bit readback logic.
- Direct call xref at `0x4414`.
- IDA type at `0x1fc8` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
