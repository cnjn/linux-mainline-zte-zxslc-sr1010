# 0x17d38 xmac_set_sopc_duplex_mode

## Status

- Status: complete
- Confidence: verified byte selector validation, diagnostic path, shared-word
  bit selection/polarity, void return, and both direct callers.
- Size: `0x68` bytes, 26 ARM64 instructions.
- Recovered signature:
  `void xmac_set_sopc_duplex_mode(u8 xmac, u32 duplex)`.

## Semantics

The function truncates `xmac` to a byte and rejects values above four after
printing `xmac_id(%d) is error`. For valid selectors, it reads the word at
`nppt_base + 0x343f0`, then updates bit `xmac + 4`:

- `duplex == 1`: clear the selected bit.
- Any other duplex value: set the selected bit.

It writes the updated word back and has no meaningful return value.

## Caller Context

Direct calls occur in:

- `xmac_init_by_work_mode @ 0x17da0`.
- `xmac_config_speed_duplex @ 0x18130`.

Both callers discard the return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. This is a volatile
MMIO-style shared-word read-modify-write; callers must provide serialization.

## Evidence

- Complete 26-instruction ARM64 body at `0x17d38` through `0x17d9c`.
- Exact byte range check against four and diagnostic string.
- Exact load/store at `nppt_base + 0x343f0`, bit index `xmac + 4`, and
  `duplex == 1` clear versus all-other-values set behavior.
- Two direct caller xrefs, neither using the return register.
- IDA function type updated at `0x17d38` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware identity and polarity of the SOPC shared-word duplex bits.
- Required synchronization for this read-modify-write.
