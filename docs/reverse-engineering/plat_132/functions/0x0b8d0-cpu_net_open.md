# 0x0b8d0 cpu_net_open

## Status

- Status: complete
- Confidence: verified control flow, ops-table slots, and raw netdev-state
  location; NAPI type labels are analyst names where IDA has no name.
- Size: `0xe0` bytes, 54 ARM64 instructions.
- Recovered signature: `int cpu_net_open(struct net_device *device)`.

## Semantics

At entry, the function performs an LDXR/STXR loop to clear bit 0 in the
64-bit word at:

```text
*(u64 *)(*(void **)(device + 0x3c0) + 0x90)
```

It then calls `netif_carrier_on(device)`. The hardware/source enable work is
name-specific:

| Device name | NAPI enables | `cpu_net_ops + 0x8` calls |
| --- | --- | --- |
| `"pon"` | `int_info`, CPU IDM context | CPU source word 0, then word 8 |
| `"idm"` | IDM context, CPU release context | CPU source word 4, then word c |
| any other name | none | none |

The `cpu_net_ops + 0x8` slot is `idm_int_enable` in the IDM ops table, as
established by the raw table and the paired stop path. The function always
returns zero and does not check carrier, NAPI, or source-unmask status.

## Pairing

`cpu_net_stop @ 0xb9b0` performs the inverse LDXR/STXR bit operation, calls
`netif_carrier_off`, disables the same NAPI contexts, and invokes
`cpu_net_ops + 0x0` (`idm_int_disable`) with the same source words.

## Evidence

- Full 54-instruction ARM64 disassembly at `0xb8d0` through `0xb9ac`.
- Direct decompilation of the paired stop function at `0xb9b0`.
- Xrefs from both CPU and IDM netdev operations tables.
- Raw IDM ops table: disable at offset 0 and enable at offset 8.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The exact netdev nested state field at `+0x3c0/+0x90` is unknown.
- Exact NAPI-context types and the source-word to hardware-event mapping remain
  pending poll-path reconstruction.
