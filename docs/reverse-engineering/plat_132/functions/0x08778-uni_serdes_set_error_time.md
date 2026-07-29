# 0x08778 uni_serdes_set_error_time

## Status

- Status: complete
- Confidence: verified mode-dependent multipliers, ordered register stores,
  high-byte clear, reported-value packing, caller context, and `int` ABI.
- Size: `0x84` bytes, 30 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_error_time(uint32_t time_units)`.

## Semantics

Calculates and programs an error-time count based on `uni_serdes_mode`:

| Mode | Multiplier | Result written to `uni_serdes_base + 0x98` |
| --- | --- | --- |
| 0-4 | `156250000` | `multiplier * time_units` modulo 32 bits |
| 5-7, 17 | `155520000` | `multiplier * time_units` modulo 32 bits |
| all other values | none | zero |

It then clears bits 24-31 at `+0xa4`. For logging, it packs the full `+0x98`
word with the low byte of `+0xa4`'s former high byte in bits 32-39; because the
preceding store clears that high byte, it normally logs the programmed 32-bit
count. It returns the final `printk` result.

## Caller Context

`uni_serdes_get_hard_prbs_cnt @ 0x97dc` calls this function at `0x9804`; its
other current xref is export metadata for `__ksymtab_uni_serdes_set_error_time`.

## Evidence

- Complete ARM64 body at `0x8778` through `0x87f8`.
- Mode read at `0x8784`; 156.25 MHz path at `0x8794`-`0x879c`; 155.52 MHz
  path at `0x87a0`-`0x87b8` for 5-7 or 17.
- Time-count store at `0x87c4`; `+0xa4` high-byte clear at `0x87c8`-`0x87d0`.
- Logged value packing at `0x87d4`-`0x87ec`; final log call at `0x87f0`.
- IDA type at `0x8778` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
