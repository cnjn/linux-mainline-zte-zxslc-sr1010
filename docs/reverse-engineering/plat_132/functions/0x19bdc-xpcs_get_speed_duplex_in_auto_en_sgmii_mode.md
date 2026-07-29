# 0x19bdc xpcs_get_speed_duplex_in_auto_en_sgmii_mode

## Status

- Status: complete
- Confidence: verified four-argument ABI, status-snapshot branches,
  output-write order, speed/duplex decoding, status return, and all direct
  callers.
- Size: `0xf8` bytes, 61 ARM64 instructions.
- Recovered signature:
  `int xpcs_get_speed_duplex_in_auto_en_sgmii_mode(u8 xmac, u32 *uni_speed, u32 *duplex, u32 *auto_status)`.

## Semantics

The helper validates `xmac <= 4`, snapshots PCS AN status offset `0x7e0008`,
and returns `-1` when bit zero is clear. When bit zero is set, it clears the
completion status through the dedicated helper and branches on cached bit four.

- Bit four clear: write zero to `*auto_status`; return zero without writing
  `*uni_speed` or `*duplex`.
- Bit four set: extract raw speed from bits two/three, write one to
  `*auto_status`, map speed through `xpcs_switch_vr_mii_an_intr_sts_speed`,
  store the mapped value to `*uni_speed`, store bit one to `*duplex`, and return
  zero.

An invalid selector logs `xmac_index(%d) is error` and returns `-1`.

## Caller Context

`xmac_speed_process_in_sgmii_auto_mode` and generic `xmac_speed_process` use
the returned output values to reconcile SGMII AN speed and duplex with XMAC
configuration.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The AN status
snapshot controls all later output writes after completion status is cleared.

## Evidence

- Complete 61-instruction ARM64 body at `0x19bdc` through `0x19cd0`.
- Four register arguments preserved into three output pointers.
- Exact `0x7e0008` status read; bit-zero and bit-four branches; raw speed and
  duplex extraction from bits two/three and bit one.
- Exhaustive direct xref query found two caller sites.
- IDA type at `0x19bdc` updated to the recovered four-argument `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS semantics of AN status bits zero and four.
- Exact physical meaning of the raw speed and duplex fields.
