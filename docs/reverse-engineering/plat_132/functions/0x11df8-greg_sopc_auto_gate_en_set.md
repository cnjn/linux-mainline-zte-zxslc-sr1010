# 0x11df8 greg_sopc_auto_gate_en_set

## Status

- Status: complete
- Confidence: verified lock/DAIF ordering, RMW, log, and semantic void ABI.
- Size: `0x68` bytes, 26 ARM64 instructions.
- Recovered signature: `void greg_sopc_auto_gate_en_set(u32 enable)`.

## Semantics

Saves DAIF and acquires the same raw lock as the getter. It replaces
`nppt_base + 0xb8` bit four with `enable & 1`, byte-release unlocks, restores
DAIF, and logs the resulting word. Inputs are normalized only through their low
bit; no error return exists.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
