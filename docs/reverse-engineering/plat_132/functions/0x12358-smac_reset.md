# 0x12358 smac_reset

## Status

- Status: complete
- Confidence: verified reset-register offset, clear/set sequence, fixed delay,
  diagnostics, void return, and all three direct callers.
- Size: `0x94` bytes, 33 ARM64 instructions.
- Recovered signature: `void smac_reset(u32 mask)`.

## Semantics

The function reads the volatile word at `nppt_base + 0x2c0004`, logs its value
and address, then computes:

```c
reset_value = original_value & ~mask;
restored_value = original_value | mask;
```

It writes `reset_value`, logs it, delays for `0x1a36f0` loop units, then writes
`restored_value` and logs it. Thus, masked bits are guaranteed set after the
operation even if they were clear in the original word. No error or timeout path
exists.

## Caller Context

Direct calls occur in:

- `nppt_smac_config_speed_duplex @ 0x123ec` for per-SMAC duplex reconfiguration.
- `nppt_smac_init @ 0x129c8` during initial SMAC setup.
- `xmac_reset @ 0x169e4` for XMAC-domain masks `0x400`/`0x800`.

All callers treat it as a void reset operation.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. This is a volatile
shared reset-word sequence; callers must provide any required serialization.

## Evidence

- Complete 33-instruction ARM64 body at `0x12358` through `0x123e8`.
- Exact register offset `0x2c0004`, `BIC`/`ORR` masks, write order, and delay
  constant `0x1a36f0`.
- Three direct caller xrefs and post-call behavior confirming return values are
  not consumed.
- IDA function type updated at `0x12358` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware reset domains and active-low/active-high semantics of this word.
- Required synchronization around concurrent reset requests.
