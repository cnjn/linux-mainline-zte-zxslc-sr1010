# 0x089b0 pin_mux_debug_clk_133_out0

## Status

- Status: complete
- Confidence: verified all four raw inputs, ordered pin/CRM RMWs, diagnostic
  read order, final log return, absent internal IDB xrefs, and exported ABI.
- Size: `0xb8` bytes, 45 ARM64 instructions.
- Recovered signature:
  `int pin_mux_debug_clk_133_out0(uint32_t pin_mux_value, uint32_t low_field, uint32_t field_16, uint32_t field_20)`.

## Semantics

Programs a debug-clock output through `top_crm_base + 0x1d0` and `pin_mux_base`:

1. Sets CRM bit 4.
2. Clears pin-mux bits 0-1, then ORs unmasked `pin_mux_value`.
3. Clears CRM bits 16-18, then ORs unmasked `field_16 << 16`.
4. Clears CRM bits 20-21, then ORs unmasked `field_20 << 20`.
5. Clears CRM bits 0-3, then ORs unmasked `low_field`.

It logs pin-mux bits 1-3, then separately reads CRM for its bits 16-18,
20-21, and 0-3 diagnostic fields. All arguments are raw OR operands rather
than field-masked values. The final CRM log result is returned.

## Caller Context

No internal IDB xrefs target this entry. It is exported through
`__ksymtab_pin_mux_debug_clk_133_out0`; vendor kallsyms also lists an external
`pin_mux_debug_clk_133_out0_store` sysfs callback in module `np`.

## Evidence

- Complete ARM64 body at `0x89b0` through `0x8a64`.
- CRM bit-4 set at `0x89c4`-`0x89cc`; pin-mux RMW at `0x89d8`-`0x89e4`.
- CRM field RMWs at `0x89ec`-`0x89f8`, `0x89fc`-`0x8a08`, and
  `0x8a0c`-`0x8a18`.
- Pin diagnostic at `0x8a1c`-`0x8a30`; three CRM diagnostic loads at
  `0x8a38`-`0x8a50`; final returned log at `0x8a58`.
- IDA type at `0x89b0` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
