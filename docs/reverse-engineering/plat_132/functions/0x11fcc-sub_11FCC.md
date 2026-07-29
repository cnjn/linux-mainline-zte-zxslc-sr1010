# 0x11fcc sub_11FCC

## Status

- Status: complete
- Confidence: verified all five instructions, no-return fall-through, system
  register sequence, target argument source, and absence of external xrefs.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature: `void sub_11FCC(void)`.

## Semantics

The function reads `ICC_PMR_EL1`, writes its low 32 bits XORed with `0xe0` back
`sopc_send_enable @ 0x11fe0`, whose byte-truncated MAC argument is therefore

The existing name is retained because no in-module caller or role establishes a
more meaningful source-level name.

## Caller Context

No in-module code or data reference targets `0x11fcc` in the current IDB. Its
only observed control-flow successor is the adjacent fall-through target
`sopc_send_enable`.

## Concurrency and Ownership

No allocation, cleanup, or ownership transfer. It touches the interrupt-priority
mask system register and issues a full system barrier before falling through.

## Evidence

- Complete five-instruction ARM64 sequence at `0x11fcc` through `0x11fdc`.
- `MRS ICC_PMR_EL1`, `EOR W1, W0, #0xe0`, two `MSR` operations, and `DSB SY`.
- Function bounds end at `0x11fe0`, proving direct fall-through into
  `sopc_send_enable` rather than a `BL` call.
- Full xref query shows no external code/data target.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`. The explicit
`sopc_send_enable` call represents the observed fall-through in source-like C.

## Open Questions

- Why this unreferenced fall-through fragment toggles and immediately restores
  the ICC priority mask.
