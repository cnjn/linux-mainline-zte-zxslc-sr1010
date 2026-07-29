# 0x01274 xepon_get_llid_state

## Status

- Status: complete
- Confidence: verified PON offset, exact byte extraction, unsigned return, and
  both direct callers.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `u32 xepon_get_llid_state(void)`.

## Semantics

Returns bits 8-15 from the 32-bit word at PON offset `0x1c0008`.

## Caller Context

Direct callers are `zx_pon_int @ 0x12bc` and `pon_is_registered @ 0x15f4`.

## Evidence

- Complete ARM64 body at `0x1274` through `0x1288`.
- Exact offset and `UBFX #8,#8` extraction.
- Exhaustive direct xref query found two callers.
- IDA type at `0x1274` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
