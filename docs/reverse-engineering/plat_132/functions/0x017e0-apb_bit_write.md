# 0x017e0 apb_bit_write

## Status

- Status: complete
- Confidence: verified four-register ABI, field-mask construction, unmasked
  value shift, APB RMW, pointer return, and all direct callers.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature:
  `volatile u32 *apb_bit_write(volatile u32 *address, u32 value, u32 width, u32 bit_offset)`.

## Semantics

Builds `field_mask = ((1U << width) - 1) << bit_offset`, clears that field in
the word at `address`, ORs `value << bit_offset` without masking `value` to the
field width, stores the result, and returns `address`.

## Caller Context

Eleven direct calls occur in `uni_serdes_reset` and two GEPHY APB setters. Only
`zte_gephy_set_ref_clk_25M` propagates the address-pointer return.

## Evidence

- Complete ARM64 body at `0x17e0` through `0x1804`.
- Exact dynamic shifts, subtract-one mask, BIC/ORR RMW, unchanged X0 return,
  and all direct xrefs.
- IDA type at `0x17e0` updated to the recovered pointer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
