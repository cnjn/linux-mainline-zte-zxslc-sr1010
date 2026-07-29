# 0x00218 ponserdes_to_xmac1_en_set

## Status

- Status: complete
- Confidence: verified zero/one shared-clock gate, inverse hardware state,
  PON/NPPT writes, global update, incidental pointer return, and caller.
- Size: `0x5c` bytes, 23 ARM64 instructions.
- Recovered signature: `u32 *ponserdes_to_xmac1_en_set(u32 enable)`.

## Semantics

Inputs zero and one call `greg_sdet_share_clk_cfg(enable)` with the original
input and ignore its `int` return. Input one clears PON offset `0x80`, clears
NPPT offset `0x2438` bit two, and sets
`g_ponserdes_to_xmac1`. Every other value writes the inverse state and clears
that global. The function returns incidental `&g_epon_deactive`.

## Caller Context

The sole direct caller is `zx_pon_probe @ 0x580`.

## Evidence

- Complete ARM64 body at `0x218` through `0x270`.
- Exact zero/one call gate, `enable != 1` and `enable == 1` predicates, PON
  offset `0x80`, NPPT offset `0x2438` bit-two RMW, and pointer return.
- Exhaustive direct xref query found only `zx_pon_probe`.
- IDA type at `0x218` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
