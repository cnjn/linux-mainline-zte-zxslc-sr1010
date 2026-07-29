# 0x00274 pon_sys_soft_reset

## Status

- Status: complete
- Confidence: verified register address, bit-31 reset pulse, delay, log order,
  zero return, and sole direct caller.
- Size: `0x94` bytes, 32 ARM64 instructions.
- Recovered signature: `int pon_sys_soft_reset(void)`.

## Semantics

Reads NPPT offset `0x2c0004`, logs the original word and address, clears bit 31,
logs and waits `__const_udelay(1718000)`, then restores bit 31, logs, and returns
zero.

## Caller Context

Its sole direct caller is `zx_pon_probe @ 0x580`.

## Concurrency and Ownership

The reset RMW sequence is not locally synchronized; other writers can race the
bit-31 pulse.

## Evidence

- Complete ARM64 body at `0x274` through `0x304`.
- Exact NPPT offset `0x2c0004`, masks, delay constant, strings, and caller xref.
- IDA type at `0x274` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
