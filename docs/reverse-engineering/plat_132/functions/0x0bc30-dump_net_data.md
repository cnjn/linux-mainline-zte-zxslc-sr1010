# 0x0bc30 dump_net_data

## Status

- Status: complete
- Confidence: verified length cap, byte formatting, line cadence, final newline,
  all direct callers, and semantically unused residual return register.
- Size: `0x88` bytes, 33 ARM64 instructions.
- Recovered signature: `void dump_net_data(const void *data, u32 length)`.

## Semantics

Prints at most the first `0xc0` bytes of `data`. Each byte is emitted through
`printk("\1c%.2x ", byte)`, with an additional `"\1c\n"` after byte indices
15, 31, and so on. It always emits a final ordinary newline, including when
`length` is zero. Therefore a length exactly divisible by 16 produces both the
per-row newline and a final extra newline.

The function has no filtering, pointer validation, allocation, lock, global
state access, or semantic return value. Callers are responsible for applying
`dump_net_check` or another debug gate before invoking it.

## Caller Context

Fourteen direct callers cover Wi-Fi trap metadata, CPU/IDM/management RX, nbuf
TX, GSO upload/segmentation, and three IDM TX submitters. The routine is a
shared debug-output sink rather than part of packet ownership or delivery.

## Evidence

- Complete 33-instruction ARM64 body at `0xbc30` through `0xbcb4`.
- Exact unsigned cap of `0xc0`, per-byte `%.2x` printk format, every-16-byte
  newline condition, and final newline.
- Fourteen direct IDA caller xrefs and no direct callee functions beyond printk.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Whether the embedded `\1c` printk level prefix has vendor-specific logging
  routing beyond standard kernel-console behavior.
