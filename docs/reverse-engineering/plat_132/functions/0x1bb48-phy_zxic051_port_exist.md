# 0x1bb48 phy_zxic051_port_exist

## Status

- Status: complete
- Confidence: verified byte input, unconditional port-use call, exact port-five
  gate, normalized boolean return, and sole direct caller.
- Size: `0x30` bytes, 12 ARM64 instructions.
- Recovered signature: `u32 phy_zxic051_port_exist(u8 phy)`.

## Semantics

The helper calls `is_certain_port_used(phy)` for every byte-truncated input. It
returns one only when `phy == 5` and that call returns nonzero; otherwise it
returns zero.

## Caller Context

Its sole direct caller is `xmac_zxic_phy_init @ 0x18348`.

## Concurrency and Ownership

No local state, locking, allocation, or cleanup exists. Port-use state is
delegated to the external helper.

## Evidence

- Complete ARM64 body at `0x1bb48` through `0x1bb74`.
- Unconditional external call, literal port-five comparison, `CCMP`, and
  normalized `CSET` return.
- Exhaustive direct xref query found only `xmac_zxic_phy_init`.
- IDA type at `0x1bb48` updated to the recovered unsigned-result signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Why only physical port five is recognized by this wrapper.
