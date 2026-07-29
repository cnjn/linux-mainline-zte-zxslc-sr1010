# 0x136dc data_padding

## Status

- Status: complete
- Confidence: verified raw skb fields, signed padding/tailroom comparison,
  conditional zero-fill, final length store, and both direct callers.
- Size: `0x64` bytes, 25 ARM64 instructions.
- Recovered signature: `void data_padding(struct zte_skb *skb)`.

## Semantics

Computes signed `padding = 60 - skb[+0xa8]`. If `skb[+0xac]` is zero, computes
signed tailroom as `skb[+0x120] - skb[+0x11c]`; otherwise tailroom remains zero.
When `padding <= tailroom`, it zeroes `padding` bytes starting at
`skb[+0x130] + skb[+0xa8]`. It always overwrites `skb[+0xa8]` with 60.

The function itself does not check that the original length is at most 60. A
negative padding reaches the sign-extended `memset` size if its signed comparison
passes; current direct callers invoke it only for short packets.

## Caller Context

Called directly by `idm_cpu_tx @ 0x14a30` and `idm_wifi_tx @ 0x14be4` when the
raw skb length is at most 59.

## Evidence

- Complete ARM64 body at `0x136dc` through `0x1373c`.
- Signed `CMP W2,W1` and `B.GT` condition before the sign-extending `memset`
  size argument.
- Direct callers at `0x14a64` and `0x14c18`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- Raw skb field names are analyst labels; only their offsets and use are
  verified.
