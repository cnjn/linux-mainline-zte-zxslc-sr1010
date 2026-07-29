# 0x0b1d0 cpu_net_register

## Status

- Status: complete
- Confidence: verified for allocation, raw field offsets, branch behavior, and
  registration cleanup; field meanings are partly inferred.
- Size: `0xd4` bytes, 53 ARM64 instructions.
- Recovered signature:
  `struct net_device *cpu_net_register(u32 type, const char *name)`.

## Semantics

The function allocates a netdev with observed arguments `(200, 1)`. A null
allocation returns null. For a successful allocation, it performs raw writes at
these offsets relative to the returned netdev object:

| Offset | Observed write |
| --- | --- |
| `0x880` | object pointer itself |
| `0x888` | input `type` |
| `0x1f8` | `idm_net_netdev_ops` only for type 3, otherwise `cpu_net_netdev_ops` |
| `0x460` | `500` |
| `0x234` | `2000` only when the copied name begins with `"omci"` |

It copies at most 15 bytes of `name` into the start of the netdev object, copies
the six-byte `default_mac` to the MAC address pointer read from offset `0x328`,
and stores the low byte of `type` into a global.

It calls `register_netdev`; only a negative result is failure. On failure it
calls `free_netdev` for that newly allocated object and returns null. Otherwise
it returns the netdev pointer. There is no validation of input type or name.

## Call Context

`cpu_net_init` calls this for types 1, 0, 2, and 3 to create `sw`, `pon`,
`omci`/`oam`, and `idm`. Type 3 is the only observed route using the IDM netdev
operations table.

## Evidence

- Full 53-instruction ARM64 disassembly at `0x0b1d0` through `0x0b2a4`.
- Direct caller xrefs from all four calls in `cpu_net_init @ 0x0e220`.
- Imports for allocation, netdev registration/free, string operations, and
  static netdev operation tables.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The exact vendor `net_device` layout and semantic names of offsets `0x880`,
  `0x888`, `0x1f8`, `0x460`, and `0x234` require vendor headers or corroborated
  field users.
- The allocation helper's complete argument ABI is not established solely by
  this binary.
