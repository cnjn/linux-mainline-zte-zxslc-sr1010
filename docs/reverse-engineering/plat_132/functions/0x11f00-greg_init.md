# 0x11f00 greg_init

## Status

- Status: complete
- Confidence: verified external-size arithmetic, stores, child call, and zero
  return.
- Size: `0x60` bytes, 24 ARM64 instructions.
- Recovered signature: `int greg_init(void)`.

## Semantics

Writes `uNORMAL_BP_SIZE | (uJUMBO_BP_SIZE << 16)` at NPPT `+0x68`. It computes
`uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE - uBP_BUFFER_OFFSET - 63` with unchecked
32-bit arithmetic, writes it to `+0x6c` and `+0x20078`, calls the fixed runt-mask
wrapper, and returns zero.

## Caller Context

`nppt_init` invokes this unconditionally between SIPC and SMAC initialization.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
