# 0x1a210 xpcs_usxgmii_mode_conf

## Status

- Status: complete
- Confidence: verified two-argument ABI, selector gate, fixed transition target,
  PCS call/RMW/reset order, post-write mode validation, cached-mode mapping,
  status returns, and all direct callers.
- Size: `0x160` bytes, 85 ARM64 instructions.
- Recovered signature: `int xpcs_usxgmii_mode_conf(u8 xmac, u32 usxg_mode)`.

## Semantics

The helper first truncates `xmac` to a byte. A selector above four logs
`xmac_index(%d) is error` and returns `-1`. For valid selectors it performs this
sequence before validating `usxg_mode`:

1. `xpcs_prepare_for_switch_mode(xmac, 5)`.
2. Write PCS type zero.
3. Enable VR-XS/PCS USXG and VSMMD1 controls.
4. RMW the PCS word at offset `0x0e001c`, replacing bits 10 through 12 with
   `usxg_mode & 7`.
5. Request VR reset with literal one, then call the reset-cleared wait helper.
   The wait result is ignored.

It then maps accepted raw mode codes to `sg_xpcs_mode[xmac]` and returns zero:

| `usxg_mode` | Cached PCS mode |
| --- | --- |
| 0, 3 | 5 |
| 1, 4 | 6 |
| 2, 5 | 7 |

All other mode codes log `unsupported usxg_mode(%d)` and return `-1`. Those
failure paths occur after the PCS enables, field RMW, reset request, and wait.

## Caller Context

Six direct calls occur in the 10G, 5G, and 2.5G USXGMII auto configuration
functions. They pass mode codes zero, one, and two respectively, then OR this
function's status with USXGMII auto-negotiation configuration status.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It updates the
shared cached mode only after accepted codes and relies on lower helpers for
reset synchronization.

## Evidence

- Complete 85-instruction ARM64 body at `0x1a210` through `0x1a36c`.
- Exact byte selector gate, fixed target-five call, and PCS helper order.
- RMW mask `0xffffe3ff` and `UBFIZ W0,W20,#10,#3` field insertion.
- Full bit-test classification for mode inputs zero through five.
- Complete decompilation of all three direct caller families confirms modes
  zero, one, and two in their respective configuration paths.
- IDA type at `0x1a210` updated to the recovered two-argument `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware decoding of USXGMII mode field bits 10 through 12.
- Why every USXGMII mode transition is prepared with fixed target mode five.
- Why reset-wait failure and unsupported mode validation occur after hardware
  changes without a rollback path.
