# 0x08a68 pin_mux_debug_clk_133_out1

## Status

- Status: complete
- Confidence: verified all three raw inputs, ordered pin/CRM RMWs, diagnostic
  read order, final log return, absent internal IDB xrefs, and exported ABI.
- Size: `0xa0` bytes, 39 ARM64 instructions.
- Recovered signature:
  `int pin_mux_debug_clk_133_out1(uint32_t pin_mux_value, uint32_t field_8, uint32_t field_24)`.

## Semantics

Programs debug-clock output 1 through `top_crm_base + 0x1d0` and
`pin_mux_base + 0x04`:

1. Sets CRM bit 12.
2. Clears pin-mux bits 12-14, then ORs unmasked `pin_mux_value << 12`.
3. Clears CRM bits 24-26, then ORs unmasked `field_24 << 24`.
4. Clears CRM bits 8-11, then ORs unmasked `field_8 << 8`.

It logs pin-mux bits 15-17, then separately reads CRM bits 24-26 and 8-11 for
the final diagnostic. Arguments are raw OR operands rather than field-masked
values, and the final CRM log result is returned.

## Caller Context

No internal IDB xrefs target this entry. It is exported through
`__ksymtab_pin_mux_debug_clk_133_out1`; vendor kallsyms also lists an external
`pin_mux_debug_clk_133_out1_store` sysfs callback in module `np`.

## Evidence

- Complete ARM64 body at `0x8a68` through `0x8b04`.
- CRM bit-12 set at `0x8a7c`-`0x8a84`; pin-mux RMW at `0x8a90`-`0x8a9c`.
- CRM field RMWs at `0x8aa4`-`0x8ab0` and `0x8ab4`-`0x8ac4`.
- Pin diagnostic at `0x8ac8`-`0x8ad8`; CRM diagnostic reads at
  `0x8ae0`-`0x8af4`; final returned log at `0x8af8`.
- IDA type at `0x8a68` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
