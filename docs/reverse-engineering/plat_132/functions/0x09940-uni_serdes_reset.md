# 0x09940 uni_serdes_reset

## Status

- Status: complete
- Confidence: verified four ordered APB sequences, unsigned input gates, delay
  constants, no-write invalid-input behavior, and exported entry. The `void`
  return type is a strong inference: the raw X0 register retains the final
  callee result, but the reset command has no observed meaningful return value.
- Size: `0x1a8` bytes, 101 ARM64 instructions.
- Recovered signature:
  `void uni_serdes_reset(uint32_t uni, uint32_t enable)`.

## Semantics

Controls one of two Uni SerDes reset sequences. `uni` and `enable` are unsigned
and only values zero and one select hardware actions. Other values log
`"uni %d or en %d is error \n"`; their subsequent comparisons select no APB
writes.

For `enable == 0`:

1. `uni == 0`: clear base bit 12, then `base + 0x400` bits 8 and 9.
2. `uni == 1`: clear `base + 0x200` bit 12, then `base + 0x400` bits 6 and 7.

For `enable == 1`:

1. `uni == 0`: set `base + 0x400` bit 9, delay `0x8312b0`, set base bit 12,
   delay `0x418958`, then set `base + 0x400` bit 8.
2. `uni == 1`: set `base + 0x400` bit 7, delay `0x8312b0`, set
   `base + 0x200` bit 12, delay `0x418958`, then set `base + 0x400` bit 6.

Every bit operation uses `apb_bit_write` with width one. Bit semantics beyond
the ordered writes are not named because the binary does not establish them.

## Caller Context

No internal IDB xrefs target this exported entry. Runtime `kallsyms` exposes
`__ksymtab_uni_serdes_reset`; module-local call sites do not establish a return
value contract.

## Evidence

- Complete ARM64 body at `0x9940` through `0x9ae4`.
- Unsigned two-argument validation at `0x9944`-`0x995c` and log at `0x9960`-
  `0x9970`.
- `enable == 0`, `uni == 0/1` sequences at `0x9978`-`0x99b8` and
  `0x99bc`-`0x9a08`.
- `enable == 1`, `uni == 0/1` sequences at `0x9a14`-`0x9a6c` and
  `0x9a70`-`0x9ad8`.
- Delays use `0x8312b0` at `0x9a34`/`0x9a94` and `0x418958` at
  `0x9a54`/`0x9ab8`.
- IDA type at `0x9940` was changed from a spurious propagated
  `apb_bit_write` pointer result to the recovered command-style `void` ABI,
  then Hex-Rays was recompiled.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
