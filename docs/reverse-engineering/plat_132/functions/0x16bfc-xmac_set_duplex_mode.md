# 0x16bfc xmac_set_duplex_mode

## Status

- Status: complete
- Confidence: verified selector handling, shared getter/setter address formulas,
  bit-24 RMW polarity, void return, and all three direct callers.
- Size: `0x88` bytes, 34 ARM64 instructions.
- Recovered signature: `void xmac_set_duplex_mode(u8 xmac, u32 duplex)`.

## Semantics

The function selects the same volatile 32-bit register used by
`xmac_get_duplex_mode`:

- XMAC `2` or `3`: raw address `((xmac + 7) << 16) + 0x140`.
- All other selectors: `nppt_base + (xmac << 18) + 0x140500`.

It clears bit 24 in the selected word, then sets the bit only when `duplex ==
0`. Any nonzero duplex input leaves the bit clear. It has no selector range
check, lock, error path, or meaningful return value.

## Caller Context

Direct calls occur in:

- `xamc_init_conf_by_speed @ 0x16d6c`.
- `xmac_init_by_work_mode @ 0x17da0`.
- `xmac_config_speed_duplex @ 0x18130`.

All three callers discard the return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. This is a volatile
MMIO-style read-modify-write; callers must provide any required serialization.

## Evidence

- Complete 34-instruction ARM64 body at `0x16bfc` through `0x16c80`.
- Exact byte-truncated `(xmac - 2) <= 1` branch and register address formulas.
- Exact `AND #0xfeffffff`, zero comparison, conditional bit-24 set, and write.
- Three direct caller xrefs, none consuming the return register.
- IDA function type updated at `0x16bfc` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware identity and polarity of bit 24 beyond its getter/setter behavior.
- Required synchronization for this register RMW.
