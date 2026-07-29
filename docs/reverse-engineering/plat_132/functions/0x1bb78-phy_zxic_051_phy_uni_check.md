# 0x1bb78 phy_zxic_051_phy_uni_check

## Status

- Status: complete
- Confidence: verified output initialization, port/PHY-ID error paths, NBASEx
  gate, indirect MDIO reads/writes, both link state machines, and sole caller.
- Size: `0x42c` bytes, 253 ARM64 instructions.
- Recovered signature:
  `int phy_zxic_051_phy_uni_check(u8 phy, u32 *state, u8 *link, u8 *speed, u8 *duplex)`.

## Semantics

The helper always zeroes its four caller outputs first. Ports zero through three
are invalid and return `-1` after an optional rate-limited log. For ports four
and above it derives a signed MDIO slot `phy - 4`; `nbaset_flag[slot] == 1`
skips all further work and returns zero. An invalid PHY ID or a `0xffff` GE
register-26 read returns `-1`.

The register-26 bit-six link state selects the link-up or link-down state
machine. Link-up obtains outer speed/duplex, updates contiguous `outerphy_link`
state, arms the APB link-down sequence, manages the register-`0x8017` flags and
MDIO scripts, packs `duplex bit 0` into output bit 10 with the speed, then calls
`speed_hold_check`. Link-down clears cached link/speed/duplex; if PHY enable
state is present, it executes the two-stage APB/GE register-zero sequence,
updates reset flags, and calls `speed_hold_check`.

## Caller Context

The sole direct caller is `phy_zxic051_check @ 0x1c0c0`, which provides PHY
port `xmac + 4` and consumes the outputs for subsequent XMAC/PCS handling.

## Concurrency and Ownership

No local lock protects the shared flags, counters, PHY-state arrays, callback
tables, or multi-write MDIO sequences. All output pointers are unconditionally
dereferenced before range validation.

## Evidence

- Complete ARM64 body at `0x1bb78` through `0x1bfa0`.
- Exact default output stores, port threshold three, NBASEx gate, PHY-ID/`0xffff`
  error paths, and signed MDIO-slot addressing.
- All link-up/down state updates, APB values `1025`/`1024`, GE bit-11 writes,
  register-`0x8017` MDIO scripts, counter transitions, and packed state output.
- Exhaustive direct xref query found only `phy_zxic051_check`.
- IDA type at `0x1bb78` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware semantics of the NBASEx gate, APB register 84 values, and the
  register-`0x8017` state flags.
