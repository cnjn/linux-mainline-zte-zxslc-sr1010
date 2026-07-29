# 0x08498 uni_serdes_set_low_power

## Status

- Status: complete
- Confidence: verified unsigned bound handling, all low-byte transactions,
  distinct invalid-mode logs, return behavior, and exported ABI.
- Size: `0x10c` bytes, 60 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_low_power(uint32_t mode)`.

## Semantics

Controls the low byte of `uni_serdes_base + 0x5c`:

| Mode | Register effect | Log |
| --- | --- | --- |
| 0 | clear low byte | `enter normal mode` |
| 1 | OR low byte with `0xff` | `enter low power mode` |
| 2 | replace low byte with `0xdd` | `enter sleep mode` |
| 3 | replace low byte with `0x22` | `enter small flow mode` |
| 4 | replace low byte with `0x33` | `enter rx en and tx off mode` |
| 5 | no MMIO | `the low power mode is error` |
| >5 | no MMIO | `LOW POWER MODE IS ERROR` |

Every path returns its selected `printk` result. The function has no internal
IDB xrefs and is exported through `__ksymtab_uni_serdes_set_low_power`.

## Evidence

- Complete ARM64 body at `0x8498` through `0x85a0`.
- Unsigned bound gate at `0x849c`-`0x84a4` and mode-zero gate at `0x84b4`.
- Mode transactions at `0x84c0`-`0x84c8`, `0x84e8`-`0x84f0`, `0x8510`-`0x8520`,
  `0x8540`-`0x8550`, and `0x8570`-`0x8580`.
- Shared final `printk` return at `0x8598`.
- IDA type at `0x8498` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
