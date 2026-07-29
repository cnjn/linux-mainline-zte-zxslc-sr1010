# 0x001ac pon_use_pll_pon_ref_from_ex_pll

## Status

- Status: complete
- Confidence: verified all four ordered calls, fixed arguments, ignored child
  returns, zero return, and absence of direct code xrefs.
- Size: `0x30` bytes, 12 ARM64 instructions.
- Recovered signature: `int pon_use_pll_pon_ref_from_ex_pll(void)`.

## Semantics

Calls `pon_set_8k_out_en(1)`, `pon_set_pin_mux_13(1)`,
`pon_set_pll_pon_ref_clock(1)`, and `pon_set_pll_pon_cfg_with_ref_clk_25M()` in
that order. It ignores all child results and returns zero.

## Caller Context

No direct code xrefs target this entry. It may be an exported or indirect PON
PLL initialization API.

## Evidence

- Complete ARM64 body at `0x1ac` through `0x1d8`.
- Exact four-call order, literal-one argument reloads, and explicit zero return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1ac` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
