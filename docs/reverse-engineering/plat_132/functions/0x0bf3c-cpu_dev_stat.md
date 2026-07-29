# 0x0bf3c cpu_dev_stat

## Status

- Status: complete
- Confidence: verified null/sentinel checks, returned offset, all direct
  callers, and absence of side effects; the returned field's original type name
  is a strong inference from caller accesses.
- Size: `0x1c` bytes, 7 ARM64 instructions.
- Recovered signature:
  `struct zte_netdev_stats *cpu_dev_stat(struct net_device *device)`.

## Semantics

Returns null when its input is null or exactly the 64-bit value
`(void *)-0x880`. For every other input it returns `device + 0x890` without
dereferencing, validating, allocating, or changing the device.

The exact sentinel test is not a range-based `IS_ERR`-style check: the ARM64
body computes `device + 0x880`, selects null only when that sum is zero, and
otherwise returns `device + 0x890`. The reason this exact invalid value can
reach the helper remains unknown.

## Caller Context

There are 31 direct call sites across RX, TX, GSO, and test paths, including
`cpu_eth_get_stats @ 0xbf58`. Callers treat a non-null result as a statistics
record and read/update 64-bit words at path-dependent offsets.

## Globals and Concurrency

No globals, MMIO, callbacks, locks, allocation, or ownership transfer. The
caller owns device lifetime and synchronization for the returned record.

## Evidence

- Complete seven-instruction ARM64 body at `0xbf3c` through `0xbf54`.
- `CBZ`, `ADD #0x890`, `CMN #0x880`, and `CSEL` establish the exact guards and
  return address.
- 31 direct IDA caller xrefs spanning CPU/IDM RX, CPU/IDM TX, GSO, and the
  netdev stats wrapper.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original type/field name at netdev offset `+0x890`.
- Provenance and intended meaning of the exact `-0x880` sentinel.
