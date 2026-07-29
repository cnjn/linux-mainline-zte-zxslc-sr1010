# 0x11ce8 greg_sdet_to_restore

## Status

- Status: complete
- Confidence: verified cached-word transaction, delay, logs, and semantic void
  ABI.
- Size: `0x6c` bytes, 27 ARM64 instructions.
- Recovered signature: `void greg_sdet_to_restore(void)`.

## Semantics

Reads and logs `nppt_base + 0x2c0004`, delays with `__const_udelay(859000)`,
writes the cached pre-delay value with bit four set, then logs that restored word.
It does not reread the register after the delay, so concurrent changes can be

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
