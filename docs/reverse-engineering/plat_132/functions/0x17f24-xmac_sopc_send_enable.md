# 0x17f24 xmac_sopc_send_enable

## Status

- Status: complete
- Confidence: verified selector truncation, ready/send register formulas,
  unbounded polling, delays, logging, void return, and sole caller.
- Size: `0xb8` bytes, 46 ARM64 instructions.
- Recovered signature: `void xmac_sopc_send_enable(u8 xmac)`.

## Semantics

The function byte-truncates the XMAC selector and repeatedly reads

```c
nppt_base + 0x34000 + 4 * ((xmac + 0xa9) & 0x1ff)
```

until bit zero becomes set. Every iteration logs the read value and logical MAC
number `xmac + 4`; an unset bit causes a fixed `0x418958`-loop delay before the
next read. There is no timeout or error return.

After readiness, it performs the same delay once more, writes one to

```c
nppt_base + 0x34000 + 4 * ((xmac + 0xb0) & 0x1ff)
```

and logs the write. The function has no meaningful return value.

## Caller Context

The sole direct caller is `xmac_config_speed_duplex @ 0x18130` at `0x181ec`,
after reset, speed configuration, and both duplex writes in its full
reconfiguration path.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs volatile
MMIO-style polling and a write; the loop can block indefinitely if readiness
never arrives.

## Evidence

- Complete 46-instruction ARM64 body at `0x17f24` through `0x17fd8`.
- Exact ready index `xmac + 0xa9`, send-enable index `xmac + 0xb0`, and base
  `nppt_base + 0x34000`.
- Exact bit-zero polling branch, repeated and post-ready delay constant
  `0x418958`, and both diagnostics.
- One direct caller xref from `xmac_config_speed_duplex`.
- IDA function type updated at `0x17f24` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware semantics of the ready and send-enable register indexes.
- Whether indefinite polling is acceptable in all runtime call contexts.
