# 0x13234 idm_stat

## Status

- Status: complete
- Confidence: verified all 35 diagnostic strings, volatile 32-bit sample
  offsets, packed-half extraction, sampling order, direct caller, and exported
  interface.
- Size: `0x334` bytes, 182 ARM64 instructions.
- Recovered signature: `void idm_stat(void)`.

## Semantics

Prints an ordered diagnostic snapshot of IDM counter/debug words through
`printk`. It reads 32-bit volatile words from `nppt_base + 0x280000` at these
offsets: `0x180`, `0x184`, `0x188`, `0x18c`, `0x190`, `0x194`, `0x198`,
`0x19c`, `0x1d8`, `0x1e4`, `0x1e8`, `0x1ec`, `0x1f8`, `0x208` through `0x228`,

For counters identified in the vendor strings as 16-bit pairs, the function
reads each word once, prints its upper half first, then its lower half. The
single-word counters and final DMA debug state each use one independent read.
It performs no locking, barrier, reset, or snapshot retry, so output lines can
observe changing hardware state. The final `printk` result remains in `W0`, but
neither the direct caller nor the exported diagnostic interface provides
evidence that this is a semantic return value.

## Caller Context

`idm_debug_stat @ 0x13568` is the sole direct module caller and invokes this
after printing software buffer-pool counters. `__ksymtab_idm_stat @ 0x1c828`
also exports the entry for external diagnostics.

## Evidence

- Complete 182-instruction ARM64 body at `0x13234` through `0x13564`.
- All 35 format strings at `0x233aa` through `0x23ce9`.
- Direct call at `0x1369c` in `idm_debug_stat`.
- Volatile `LDR Wn` access pattern through imported `nppt_base`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- Vendor labels establish diagnostic names but not formal hardware register
  definitions, counter wrap behavior, or cross-counter consistency guarantees.
