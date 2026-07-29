# 0x11d98 greg_sopc_auto_gate_en_get

## Status

- Status: complete
- Confidence: verified DAIF/lock sequence, bit extraction, log, and return.
- Size: `0x60` bytes, 24 ARM64 instructions.
- Recovered signature: `u32 greg_sopc_auto_gate_en_get(void)`.

## Semantics

Saves DAIF, locks `nppt_glb_auto_gate_lock`, reads `nppt_base + 0xb8`, extracts
bit four, byte-release unlocks, restores DAIF, logs raw/enabled values, and
returns zero or one. The MMIO read is serialized only with users of this local
lock.

## Caller Context

CPU-133 SMAC/XMAC speed reconfiguration paths save the value and restore gate
enable only when this function returned one.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
