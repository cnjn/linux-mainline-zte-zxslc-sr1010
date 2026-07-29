# 0x0128c xgpon_get_onu_state

## Status

- Status: complete
- Confidence: verified PON offset, three-bit mask, unsigned return, and both
  direct callers.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `u32 xgpon_get_onu_state(void)`.

## Semantics

Returns the low three bits from the 32-bit word at PON offset `0x59400`.

## Caller Context

Direct callers are `zx_pon_int @ 0x12bc` and `pon_is_registered @ 0x15f4`.

## Evidence

- Complete ARM64 body at `0x128c` through `0x12a0`.
- Exact offset and `AND #7` extraction.
- Exhaustive direct xref query found two callers.
- IDA type at `0x128c` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
