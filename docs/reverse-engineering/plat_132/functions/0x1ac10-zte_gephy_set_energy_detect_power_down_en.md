# 0x1ac10 zte_gephy_set_energy_detect_power_down_en

## Status

- Status: complete
- Confidence: verified byte arguments, register selection, raw-byte shift,
  bit-three clear mask, zero return, and no direct xrefs.
- Size: `0x48` bytes, 18 ARM64 instructions.
- Recovered signature:
  `int zte_gephy_set_energy_detect_power_down_en(u8 phy, u8 enable)`.

## Semantics

The function reads MDIO register 21, clears bit three, then ORs the entire
byte-truncated `enable` value shifted left by three before writing register 21.
It returns zero:

```c
value = (value & 0xfff7U) | ((u32)enable << 3);
```

`enable` is not masked to bit zero; values larger than one can alter bits above
bit three.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or error checking. The MDIO RMW is not
internally serialized.

## Evidence

- Complete ARM64 body at `0x1ac10` through `0x1ac54`.
- `UXTB` input conversion followed by raw `ORR W2,W2,W19,LSL#3`.
- Exact register 21, `0xfffffff7` mask, and zero return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1ac10` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether callers contractually restrict enable to zero or one.
