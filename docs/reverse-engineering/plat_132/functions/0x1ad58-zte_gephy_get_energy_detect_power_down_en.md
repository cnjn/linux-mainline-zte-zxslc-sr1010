# 0x1ad58 zte_gephy_get_energy_detect_power_down_en

## Status

- Status: complete
- Confidence: verified null handling, discarded MDIO read, zero output store,
  returns, and no direct xrefs.
- Size: `0x48` bytes, 17 ARM64 instructions.
- Recovered signature:
  `int zte_gephy_get_energy_detect_power_down_en(u8 phy, u8 *enable)`.

## Semantics

The function rejects a null output pointer, logs `"power_down_en is null\n"`,
and returns `-1`. With a valid pointer it reads MDIO register 21 but discards
the result, writes zero to the output byte, and returns zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

The caller owns output storage. No local lock or MDIO error handling exists.

## Evidence

- Complete ARM64 body at `0x1ad58` through `0x1ad9c`.
- Null output branch and exact log string.
- Direct register-21 read followed by `STRB WZR`, with no data dependency.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1ad58` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether the discarded MDIO read clears hardware state or is a vestigial read.
