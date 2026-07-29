# 0x07f20 pon_pll_cfg

## Status

- Status: complete
- Confidence: verified mode ranges, CPU-132 conditional profiles, all CRM RMW
  and literal-store ordering, caller context, and constant-zero `int` ABI.
- Size: `0x168` bytes, 82 ARM64 instructions.
- Recovered signature: `int pon_pll_cfg(uint32_t mode)`.

## Semantics

Selects PON CRM PLL programming by mode range:

| Mode range | Log | Base CRM sequence |
| --- | --- | --- |
| 0-4 | EPON | Set `+0x10` bits 4-5 to binary 2, program `+0xc0/+0xc4` by CPU type |
| 5-7 | GPON | Same `+0x10` field, distinct `+0xc0/+0xc4` values by CPU type |
| 8-16 | Ethernet | Clear `+0x10` bits 4-5, then program Ethernet `+0xc0/+0xc4` values |
| other | none | return zero without CRM writes |

All valid ranges then set bit 9 and clear bit 8 of `top_crm_base + 0xc`. The
non-CPU-132 EPON and GPON paths first OR bit 28 into `+0xc4`, then overwrite
that register with a literal; this otherwise redundant sequence is preserved.
The function always returns zero.

## Caller Context

`zx_pon_clk_reset_init @ 0x8088` is the sole direct caller. It invokes this
before reset/clock control and propagates the constant-zero result only through
its own broader initialization flow.

## Evidence

- Complete ARM64 body at `0x7f20` through `0x8084`.
- Mode range gates: EPON `0x7f24`-`0x7f30`, GPON `0x7f8c`-`0x7f94`, Ethernet
  `0x8014`-`0x801c`.
- CPU-132 EPON values at `0x7f64`-`0x7fe8`; non-132 EPON sequence at
  `0x7f78`-`0x805c`; corresponding GPON paths at `0x7fcc`-`0x7fec` and
  `0x7ff0`-`0x805c`.
- Ethernet profile at `0x8034`-`0x805c`; common `+0xc` RMWs at
  `0x8060`-`0x8074`.
- Sole caller at `0x809c`.
- IDA type at `0x7f20` set to the recovered semantic signature and Hex-Rays
  cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
