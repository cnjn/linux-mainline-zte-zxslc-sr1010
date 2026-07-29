# 0x11c08 greg_sdet_to_reset

## Status

- Status: complete
- Confidence: verified raw RMW, logs, and semantic void ABI.
- Size: `0x5c` bytes, 23 ARM64 instructions.
- Recovered signature: `void greg_sdet_to_reset(void)`.

## Semantics

Reads `nppt_base + 0x2c0004`, logs the raw word/address, clears bit four and
writes the result, then logs the written value. It has no delay, barrier, lock,

## Caller Context

`zx_pon_probe` calls it before its external delay, PON soft reset, SIPC init,

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
