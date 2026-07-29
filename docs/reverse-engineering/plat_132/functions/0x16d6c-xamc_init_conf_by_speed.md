# 0x16d6c xamc_init_conf_by_speed

## Status

- Status: complete
- Confidence: verified selector windows, all fixed offsets/values, call order,
  final read-modify-write, void return, and all 19 direct callers.
- Size: `0x178` bytes, 94 ARM64 instructions.
- Recovered signature:
  `void xamc_init_conf_by_speed(u8 xmac, u32 speed)`.

## Semantics

The function uses one of two selector-specific register windows:

| Selector | Base | Subsequent offsets |
| --- | --- | --- |
| XMAC `2`/`3` | `(xmac + 7) << 16` | `0x4`, `0x8`, `0xa0`, `0xd00` |
| all others | `nppt_base + (xmac << 18) + 0x140000` | `0x10`, `0x20`, `0x280`, `0x3400` |

It writes `0x00010000` at the base, calls `xmac_set_speed_sel(xmac, speed)`,
`0x3e800086`, `0x80000001`, and `2` at the first three window-specific offsets.
Finally it reads the fourth offset and writes the value ORed with `0x200`.

There is no selector range check, lock, error path, or meaningful return value.

## Caller Context

Nineteen direct call sites use this common XMAC configuration primitive:

- Both CPU-sequence paths of each recovered XMAC mode setter
  (`xmac_sgmii_conf`, 10G/5GBASE-R, 2.5G/1GBASE-X, all three USXGMII modes, and
  HSGMII).
- The full duplex-change path of `xmac_config_speed_duplex @ 0x18130`.

All callers treat it as a void configuration step.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs several
volatile MMIO-style writes and a final read-modify-write; callers must provide
any required serialization.

## Evidence

- Complete 94-instruction ARM64 body at `0x16d6c` through `0x16ee0`.
- Exact byte selector split, all special/NPPT-relative address formulas, and
  fixed write constants.
- Exact ordering of base write, speed/duplex helper calls, three later writes,
  and final `ORR #0x200` RMW.
- Nineteen direct caller xrefs spanning all recovered mode setters and runtime
  duplex reconfiguration.
- IDA function type updated at `0x16d6c` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meanings of the raw windows, fixed configuration values, and bit 9.
- Required serialization across the multi-register initialization sequence.
