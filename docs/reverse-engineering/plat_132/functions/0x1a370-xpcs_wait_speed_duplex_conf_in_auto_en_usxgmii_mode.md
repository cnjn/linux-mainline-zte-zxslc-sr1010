# 0x1a370 xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode

## Status

- Status: complete
- Confidence: verified byte selector, local output initialization, retry/delay
  behavior, status return, timeout log, and no direct code xrefs.
- Size: `0xb0` bytes, 43 ARM64 instructions.
- Recovered signature:
  `int xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode(u8 xmac)`.

## Semantics

The helper truncates its selector to a byte, initializes three local words
(`uni_speed`, `duplex`, and `auto_status`) to zero, then attempts
`xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode` up to 400 times with pointers

It returns immediately when the callee returns zero. On a nonzero result it
calls `__const_udelay(859000)` and retries. After the 400th failed attempt and
delay, it logs:

```
in usxgmii mode and auto is enable, pcs in mac side can't get speed and duplex
```

and returns the last nonzero callee status. The three local output values are
never returned or otherwise consumed by this wrapper.

## Caller Context

No direct code xrefs exist in the current IDB. The function remains a complete
callable vendor helper and may be reached indirectly or be unused in this build.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The PCS interactions
and output-side effects belong to the delegated auto-enable handler.

## Evidence

- Complete 43-instruction ARM64 body at `0x1a370` through `0x1a41c`.
- Local-word initialization, 400-counter, and per-failure `0x0d1b78` delay.
- Exact success exit, timeout log path, and final status return.
- Exhaustive direct xref query returned zero caller sites.
- IDA type at `0x1a370` updated to the recovered byte-selector `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Why the wrapper's local output values are never consumed.
- Whether the function is indirectly invoked or unused in this module build.
