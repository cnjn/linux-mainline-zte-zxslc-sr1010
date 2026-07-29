# 0x0bf58 cpu_eth_get_stats

## Status

- Status: complete
- Confidence: verified forwarding call, return propagation, static-table
  references, and absence of local side effects.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature:
  `struct zte_netdev_stats *cpu_eth_get_stats(struct net_device *device)`.

## Semantics

This is a direct wrapper over `cpu_dev_stat(device)`. It forwards the device
argument unchanged and returns the helper's value unchanged, including null for
the helper's null or exact `-0x880` sentinel input. It adds no validation,
counter access, allocation, lock, callback, or ownership behavior.

## Registration Context

Two data entries contain this function pointer: `0x1dd18` and `0x1df38`, at
offset `+0x20` in two otherwise parallel static operation tables beginning at
`0x1dcf8` and `0x1df18`. The table-field identity is strongly inferred to be a
netdev statistics accessor from this function's behavior and existing table
usage, but no vendor type declaration establishes its original field name.

## Evidence

- Complete five-instruction ARM64 body at `0xbf58` through `0xbf68`.
- The sole `BL cpu_dev_stat` preserves the incoming `x0` and its return register.
- Two direct data xrefs and raw table values at `0x1dd18` and `0x1df38`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original operation-table field name and complete table layout.
- Whether any external caller reaches this pointer independently of the two
  observed operation tables.
