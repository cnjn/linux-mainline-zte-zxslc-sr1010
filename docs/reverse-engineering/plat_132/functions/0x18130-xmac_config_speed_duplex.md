# 0x18130 xmac_config_speed_duplex

## Status

- Status: complete
- Confidence: verified input truncation/use, speed mapping, duplex fast path,
  CPU-133 gate sequence, full reconfiguration order, void return, and caller.
- Size: `0x110` bytes, 68 ARM64 instructions.
- Recovered signature:
  `void xmac_config_speed_duplex(u8 xmac, u32 uni_speed, u32 duplex)`.

## Semantics

The helper initializes a mapped XMAC speed to one and a current-duplex output
to two. It maps `uni_speed`, reads the current XMAC duplex, then branches:

- If current duplex equals the requested `duplex`, it only calls
  `xmac_set_speed_sel(xmac, mapped_speed)` and returns.
- Otherwise it conditionally manages the CPU-133 SOPC auto-gate, then executes
  this exact order: `xmac_reset`, `xamc_init_conf_by_speed`,
  `xmac_set_duplex_mode`, `xmac_set_sopc_duplex_mode`, and
  `xmac_sopc_send_enable`.

For the full reconfiguration path, it calls `greg_sopc_auto_gate_en_get` only
when the first `isCpuType_133()` result equals one. If that value is exactly
one, it disables the gate before reset. After all reconfiguration calls, it
re-enables the gate only if the saved value was one and a second
`isCpuType_133()` call also returns one.

The caller supplies a zero-extended speed byte, but this function consumes its
full `W1` register value when it invokes the mapper, so the original source
declaration width is not independently verified; the reconstruction uses `u32`.

## Caller Context

The sole direct caller is `check_phy @ 0x126e4` at `0x12830`, on its CPU-133/129
XMAC link-up path after it decodes the PHY status speed and duplex values.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It coordinates
shared hardware state entirely through delegated XMAC and gate helpers.

## Evidence

- Complete 68-instruction ARM64 body at `0x18130` through `0x1823c`.
- Initial mapped-speed/current-duplex words and exact calls to the mapper and
  duplex reader.
- Exact equality fast path and five-call reconfiguration sequence.
- Two separate CPU-133 tests, exact saved-gate comparison against one, and
  disable/restore call sites.
- One direct caller xref from `check_phy`.
- IDA function type updated at `0x18130` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware role of the SOPC auto-gate and why it is limited to CPU 133.
- Original declared width of the speed argument.
