# 0x1a420 xpcs_init

## Status

- Status: complete
- Confidence: verified selector gate, both reset polls, retry/delay behavior,
  error paths, final control writes, return values, and sole direct caller.
- Size: `0x130` bytes, 74 ARM64 instructions.
- Recovered signature: `int xpcs_init(u8 xmac)`.

## Semantics

The function accepts only XMAC selectors zero through four. It waits for bit 15
to clear at PCS offsets `0x0c0000` and then `0x7c0000`, polling each up to 400
times with `__const_udelay(859000)` between busy reads. A timeout logs a
register-specific message and returns `-1`; a valid completed initialization
clears the TX-config and MII-control bits through the two VR-MII AN helpers and
returns zero.

Selectors two and three use raw per-XMAC windows. The other valid selectors use
the `xmac0_pcs_base`-relative PCS windows.

## Caller Context

`xmac_init_by_work_mode @ 0x17da0` calls this before `xmac_reset(xmac)` and
discards its status. Failure therefore does not stop that caller's subsequent
reset operation.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. Each timeout may
block the caller for 400 delay intervals.

## Evidence

- Complete ARM64 body at `0x1a420` through `0x1a54c`.
- Selector gate `xmac <= 4`, both bit-15 polling loops, retry count 400, and
  delay constant `859000`.
- Exact error strings for SR-XS/PCS and SR-MII reset timeouts.
- Direct calls to `xpcs_set_vr_mii_an_ctrl_tx_config(xmac, 0)` and
  `xpcs_set_vr_mii_an_ctrl_mii_ctrl(xmac, 0)` only after both waits succeed.
- Exhaustive direct xref query found only `xmac_init_by_work_mode @ 0x17da0`.
- IDA type at `0x1a420` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- The reset ownership and required recovery action when this caller ignores a
  timeout status.
