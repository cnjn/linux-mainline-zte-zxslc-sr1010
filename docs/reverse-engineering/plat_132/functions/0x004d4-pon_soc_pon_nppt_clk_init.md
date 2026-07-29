# 0x004d4 pon_soc_pon_nppt_clk_init

## Status

- Status: complete
- Confidence: verified CPU branch, both CRM writes, diagnostic rereads, zero
  return, and sole direct caller.
- Size: `0x74` bytes, 26 ARM64 instructions.
- Recovered signature: `int pon_soc_pon_nppt_clk_init(void)`.

## Semantics

Reads CRM offsets `0xc` and `0x48`. CPU 129 applies mask/value
`0xff9fffcf`/`0x00400030` to the mux word; other CPUs use
`0xf8ffffff`/`0x06000000`. It sets offset `0x48` bit 10, writes both words,
rereads them for a diagnostic log, and returns zero.

## Caller Context

Its sole direct caller is `zx_pon_probe @ 0x580`.

## Evidence

- Complete ARM64 body at `0x4d4` through `0x544`.
- Exact predicate, masks, values, offsets, log string, rereads, and caller xref.
- IDA type at `0x4d4` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
