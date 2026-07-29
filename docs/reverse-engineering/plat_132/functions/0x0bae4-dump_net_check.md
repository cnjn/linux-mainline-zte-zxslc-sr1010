# 0x0bae4 dump_net_check

## Status

- Status: complete
- Confidence: verified mode dispatch, bounds checks, two 64-bit masked
  comparisons, return convention, globals, and all direct callers; condition
  field labels are strong inference from the paired setter and use.
- Size: `0x14c` bytes, 83 ARM64 instructions.
- Recovered signature: `int dump_net_check(const void *data, u32 length)`.

## Semantics

Returns zero when a packet is accepted for debug output and `-1` when it is
rejected or a required eight-byte read falls outside `length`. It reads the
unlocked state configured by `dump_net_condition_set`:

| `g_net_dump_select` | Result |
| --- | --- |
| 0 | Accept all without reading `data`. |
| 1 | Accept only when condition 0 matches. |
| 2 | Accept only when condition 0 does not match. |
| 3 | Accept when either condition matches. |
| 4 | Accept only when both conditions match. |
| Other | Accept all without reading `data`. |

For each required condition, it first checks
`(u64)shift + 8 <= length`, then evaluates:

```c
(*(const u64 *)(data + shift) & mask) == value
```

Modes 3 and 4 validate both read ranges before evaluating either condition.
There is no null-data check; modes that require a read can dereference a null
pointer if the length check passes.

## Caller Context

Nine direct call sites use this predicate to gate debug dumps: two paths in
`idm_set_wifi_trap_info`, CPU/IDM/management RX paths, nbuf TX, and the IDM CPU,
management, and Wi-Fi TX submitters. A zero result enables their corresponding
debug print/data-dump behavior.

## Globals and Concurrency

- Reads `g_net_dump_select` and both 24-byte `g_net_dump_condition` records.
- No writes, locks, barriers, allocation, MMIO, or callbacks.
- Concurrent `dump_net_condition_set` updates can yield mixed mask/value/shift
  observations because the configuration is read without synchronization.

## Evidence

- Complete 83-instruction ARM64 body at `0xbae4` through `0xbc2c`.
- Direct cross-check with `dump_net_condition_set @ 0xb2ec` establishes both
  condition record fields and match-mode values.
- Nine direct IDA caller xrefs across RX/trap/TX debug paths.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Intended payload endianness contract for externally supplied mask/value input.
- Whether the unchecked data pointer is safe under every vendor debug call path.
