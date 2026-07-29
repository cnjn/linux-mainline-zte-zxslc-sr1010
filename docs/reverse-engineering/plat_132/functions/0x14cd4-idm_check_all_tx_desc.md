# 0x14cd4 idm_check_all_tx_desc

## Status

- Status: complete
- Confidence: verified queue range check, ring-base load, loop, raw length
  extraction, diagnostics, and all return paths from complete ARM64 assembly.
- Size: `0xb4` bytes, 43 ARM64 instructions.
- Recovered signature: `uint32_t idm_check_all_tx_desc(uint32_t queue_index)`.

## Semantics

For queue indices `0..3`, loads `idm_tx_q[queue_index].descriptor_base` and
loops from zero to the live global `uIDM_TX_QUEUE_DESC_DEPTH`. On each iteration
it tests only the same first descriptor's `((u16(+0x04) >> 1) & 0x3fff)` length.
Lengths at most 15 cause a ratelimited `printk` and the full `dump_tx_desc`
diagnostic.

The loop deliberately has no descriptor-pointer increment or `32 * index`
offset. Consequently, it repeats the first descriptor check and may emit the
same descriptor diagnostic once per loop iteration. This is verified machine
code behavior, not a reconstruction omission.

For an invalid queue index it returns that input unchanged. For a valid queue it
returns the final live descriptor-depth value. It has no lock, barrier, or
hardware write.

## Caller Context

There are no direct code or data xrefs to the function within this module. It is
therefore an unused internal diagnostic helper in the recovered module image;
external or indirect use is not established.

## Evidence

- Complete ARM64 body at `0x14cd4` through `0x14d84`.
- `CMP W0, #3` range guard and `LDR X22, [X1,X0]` queue-base load after exact
  40-byte queue stride calculation.
- Loop reads `LDRH W0, [X22,#4]` every iteration and contains no write or
  increment of `X22`.
- Diagnostic format string at `0x23f65`, function-name string at `0x1e5d0`, and
  direct `dump_tx_desc` call at `0x14d64`.
- IDA xref query found no incoming module references.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
