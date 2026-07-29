# 0x14b8c dump_tx_desc_wifi

## Status

- Status: complete
- Confidence: verified both format strings, raw reads/masks, void ABI, and sole
  direct caller.
- Size: `0x58` bytes, 22 ARM64 instructions.
- Recovered signature: `void dump_tx_desc_wifi(const void *descriptor)`.

## Semantics

Prints two diagnostic lines without modifying the descriptor. The first prints
the pointer, words `+0x00/+0x04/+0x08`, byte `+0x07 & 0x3f` (`p`), and
`(u16(+0x04) >> 1) & 0x3fff` (`l`). The second prints words
`+0x0c/+0x10/+0x14/+0x18` as `soft define` values. The final `printk` result is
residual, not a semantic return value.

## Caller Context

`idm_wifi_tx @ 0x14be4` is the sole direct module caller, at `0x14c90`.

## Evidence

- Complete ARM64 body at `0x14b8c` through `0x14be0`.
- Wi-Fi format string at `0x23f1c` and shared `soft define` string at `0x23e9b`.
- Direct xref at `0x14c90`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
