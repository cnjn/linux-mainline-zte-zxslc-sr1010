# 0x04378 serdes_set_lane_mode

## Status

- Status: complete
- Confidence: verified CPU predicate, CPU-gated RMW location and mask,
  unmasked input propagation, residual-return behavior, and export context.
- Size: `0x40` bytes, 16 ARM64 instructions.
- Recovered signature: `void serdes_set_lane_mode(uint32_t lane_mode)`.

## Semantics

Only CPU type 133 is affected. When `isCpuType_133() == 1`, this function reads
`pon_serdes_base + 0x94`, clears bits `2:0`, ORs the raw `lane_mode` parameter,

The input is not masked to three bits before the OR. Thus inputs with bits above
2 set can alter fields outside the nominal lane-mode field; the reconstruction
preserves this exact vendor behavior.

## Return Semantics

When CPU type is not 133, `RET` retains the predicate result. When it is 133,
it retains the pre-OR masked register value in `w0`; the stored value is in
`w19`. Neither result is a coherent status, so the recovered semantic ABI is
`void`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. Vendor
`system/proc/kallsyms` has `__ksymtab_serdes_set_lane_mode` and global text
symbol `serdes_set_lane_mode [plat_132]`, confirming this API is exported.

## Evidence

- Complete ARM64 body at `0x4378` through `0x43b4`.
- `BL isCpuType_133`, `CMP W0, #1`, and `B.NE 0x43ac` form the sole gate.
- `LDR` / `AND #0xfffffff8` / `ORR W19, W0, W19` / `STR` at
  `0x439c`-`0x43a8` demonstrates both the field and unmasked input.
- IDA type at `0x4378` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
