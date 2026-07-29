# 0x0b9b0 cpu_net_stop

## Status

- Status: complete
- Confidence: verified control flow, ops-table slot, and raw netdev-state
  location; NAPI type labels are analyst names where IDA has no name.
- Size: `0xdc` bytes, 53 ARM64 instructions.
- Recovered signature: `int cpu_net_stop(struct net_device *device)`.

## Semantics

The function is the direct counterpart to `cpu_net_open`:

1. Atomically sets bit 0 in the 64-bit nested netdev state word at
   `*(u64 *)(*(void **)(device + 0x3c0) + 0x90)` using LDXR/STXR.
2. Calls `netif_carrier_off(device)`.
3. For `"pon"`, disables `int_info` and the CPU IDM NAPI context while calling
   `cpu_net_ops + 0x0` for source words 0 and 8.
4. For `"idm"`, disables the IDM and CPU release NAPI contexts while calling
   `cpu_net_ops + 0x0` for source words 4 and c.
5. Returns zero for every device name.

The `cpu_net_ops + 0x0` slot is `idm_int_disable` in the IDM ops table. Names
other than `pon` and `idm` receive carrier/state transition only.

## Pairing and Ownership

`cpu_net_open` uses the inverse state-bit update, carrier-on, NAPI enable, and
`cpu_net_ops + 0x8` (`idm_int_enable`) calls. Neither function frees task
queues, NAPI contexts, timers, or netdevs; stop only quiesces selected sources.

## Evidence

- Full 53-instruction ARM64 disassembly at `0xb9b0` through `0xba88`.
- Direct decompilation of paired open function at `0xb8d0`.
- Netdev ops-table xrefs and raw IDM ops-table data.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The exact nested netdev state field remains unknown.
- The source-mask lifecycle after NAPI poll completion remains unresolved.
