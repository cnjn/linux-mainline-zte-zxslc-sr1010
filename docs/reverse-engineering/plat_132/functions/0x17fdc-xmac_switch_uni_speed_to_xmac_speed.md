# 0x17fdc xmac_switch_uni_speed_to_xmac_speed

## Status

- Status: complete
- Confidence: verified all six mappings, the conditional speed-four mapping,
  no-write default path, void return, and all six direct callers.
- Size: `0x7c` bytes, 29 ARM64 instructions.
- Recovered signature:
  `void xmac_switch_uni_speed_to_xmac_speed(u8 xmac, u32 uni_speed,
  u32 *xmac_speed)`.

## Semantics

The function truncates `xmac` to a byte and conditionally writes an XMAC
speed-select value through `xmac_speed`:

| UNI speed | XMAC speed-select |
| --- | --- |
| `1` | `7` |
| `2` | `4` |
| `3` | `3` |
| `4` | `2` when `sg_xmac_work_mode[xmac]` is `8` or `9`; otherwise `6` |
| `5` | `5` |
| `6` | `0` |

Inputs outside one through six do not write the output pointer. Callers must
therefore initialize their output slot when they require a defined value for
such an input. No return register is consumed at any of the six direct callers.

## Caller Context

Direct calls occur in:

- `xmac_speed_process_in_sgmii_auto_mode @ 0x18058`.
- `xmac_config_speed_duplex @ 0x18130`.
- `xmac_speed_process_in_usxgmii_auto_mode @ 0x18530`.
- Both SGMII and USXGMII paths in `xmac_speed_process @ 0x1860c`.
- `phy_051_set_xmac_speed @ 0x1c00c`.

Each caller supplies a local output word and consumes that word after the call.

## Concurrency and Ownership

No local lock, allocation, cleanup, ownership transfer, or MMIO occurs. The
function reads one work-mode cache entry and conditionally writes caller-owned
storage.

## Evidence

- Complete 29-instruction ARM64 body at `0x17fdc` through `0x18054`.
- Six-case jump table keyed by `uni_speed - 1` and exact output constants.
- Raw unsigned `(work_mode - 8) <= 1` test for the input-four special case.
- Six direct code xrefs; each caller's post-call instructions read its output
  slot rather than the function return register.
- IDA function type updated at `0x17fdc` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware encoding behind the speed-select values.
- Why HSGMII work modes eight/nine require a distinct encoding for UNI speed
  four.
