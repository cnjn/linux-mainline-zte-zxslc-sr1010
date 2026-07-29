# 0x00450 nppt_idm_cci_enable

## Status

- Status: complete
- Confidence: verified both fixed system-control writes, log string, tail
  return, and sole direct caller.
- Size: `0x30` bytes, 11 ARM64 instructions.
- Recovered signature: `int nppt_idm_cci_enable(void)`.

## Semantics

Writes `0x00200020` to system-control offsets `0x78` and `0x7c`, then returns
the result of `printk("idm cci enable\n")`.

## Caller Context

Its sole direct caller is `zx_pon_probe @ 0x580`.

## Evidence

- Complete ARM64 body at `0x450` through `0x47c`.
- Exact offsets, fixed value, log string, and tail return.
- Exhaustive direct xref query found only `zx_pon_probe`.
- IDA type at `0x450` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
