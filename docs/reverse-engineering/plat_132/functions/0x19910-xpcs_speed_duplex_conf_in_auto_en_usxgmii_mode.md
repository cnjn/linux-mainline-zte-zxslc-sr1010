# 0x19910 xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode

## Status

- Status: complete
- Confidence: verified four-argument ABI, status-snapshot branches,
  output-write order, speed/duplex decode, PCS/reset sequence, wait status
  return, and all direct callers.
- Size: `0x140` bytes, 79 ARM64 instructions.
- Recovered signature:
  `int xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode(u8 xmac, u32 *uni_speed, u32 *duplex, u32 *auto_status)`.

## Semantics

The helper validates `xmac <= 4`, then snapshots the selector-specific PCS AN
status word at offset `0x7e0008`. An invalid selector logs and returns `-1`.
Bit zero clear also returns `-1` without writing any output. When bit zero is
set, the helper clears the completion status using the cached snapshot.

When cached bit 14 is clear, it writes zero to `*auto_status` and returns zero;
`*uni_speed` and `*duplex` are untouched. When cached bit 14 is set, it:

1. Extracts reported speed from bits 10 through 12 and duplex from bit 13.
2. Writes one to `*auto_status`.
3. Converts the reported speed in place through
   `xpcs_switch_vr_mii_an_intr_sts_speed`.
4. Writes converted speed and extracted duplex to SR-MII controls.
5. Delays `859000`, then enables USRA reset.
6. Commits converted speed to `*uni_speed` and duplex to `*duplex`.
7. Returns the VR-reset wait result.

The post-reset outputs are committed before the wait, so a timeout does not
roll them back.

## Caller Context

Three direct callers use this helper:

- `xmac_speed_process_in_usxgmii_auto_mode` and generic `xmac_speed_process`
  use its output values to coordinate XMAC speed/duplex updates.
- `xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode` uses it within its
  higher-level wait/retry path.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The initial AN
status snapshot deliberately governs all later branches even after completion
status is cleared. Output pointer ownership belongs to callers.

## Evidence

- Complete 79-instruction ARM64 body at `0x19910` through `0x19a4c`.
- Four register arguments preserved into the selector and output pointers.
- Exact `0x7e0008` status read, bit-zero and bit-14 tests, and output stores.
- Delay/reset/wait ordering and output commits before the wait call.
- Exhaustive direct xref query found three caller sites.
- IDA type at `0x19910` updated to the recovered four-argument `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS semantics of the AN status bits zero and 14.
- Exact hardware mapping performed by the delegated speed conversion helper.
