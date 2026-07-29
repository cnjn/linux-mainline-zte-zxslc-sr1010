# 0x0b2ec dump_net_condition_set

## Status

- Status: complete
- Confidence: verified argument truncation, condition-record writes, byte swaps,
  print-mode messages, condition dump behavior, export caller, and predicate
  consumer; field labels are strong inference from observed use.
- Size: `0x1c4` bytes, 105 ARM64 instructions.
- Recovered signature:
  `void dump_net_condition_set(u8 print_type, u8 condition_index, u64 mask, u64 value, u32 shift)`.

## Semantics

Unconditionally stores the low byte of `print_type` in `g_net_dump_select`. If
the low byte of `condition_index` exceeds one, it returns immediately, leaving
the selected mode changed but leaving both condition records untouched.

For condition index 0 or 1, it writes one 24-byte record:

| Offset | Stored value |
| --- | --- |
| `+0x0` | byte-swapped `mask` |
| `+0x8` | byte-swapped `value` |
| `+0x10` | unmodified `shift` |

`dump_net_check @ 0xbae4` shows that each `shift` is used as a byte offset for a
64-bit packet-data load, despite its vendor configuration label. The selected
mode controls that consumer as follows: 0 accepts all packets; 1 requires
condition 0; 2 requires condition 0 not to match; 3 accepts either condition;
and 4 requires both conditions.

The function prints a mode message for values 0 through 4, then dumps both raw
condition records. The condition-1 log line uses condition 0's shift value, an
observed binary logging defect; it does not change the stored condition-1 shift.

## Caller Context

There are no direct in-module callers. The function is exported and `np.ko`
imports it. Its `np_idm_test_print_config_store @ 0xb87a8` parses five values
from the `print_config` configuration input and invokes this function only when
the parsed print type is at most four. Captured `np.ko` help text describes:

```text
echo [print_type] [condition num] [mask] [value] [shift]> print_config
```

The configured predicate is consumed by `dump_net_check`, which gates debug
dump paths across RX, management, and TX functions.

## Concurrency and Ownership

- No lock, atomic operation, allocation, or ownership transfer.
- The setter can race packet-path `dump_net_check` readers and multiword
  condition-record updates are not published atomically.

## Evidence

- Complete 105-instruction ARM64 body at `0xb2ec` through `0xb4ac`.
- Two direct `__fswab64` calls and exact 24-byte indexed stores.
- Runtime export, `np.ko` undefined-symbol evidence, and its relocation call
  site at `0xb8808`.
- Direct `dump_net_check` reconstruction establishes condition offsets, matching
  modes, and all predicate global fields.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact intended synchronization policy for live debug-filter changes.
- Whether any external caller uses print types above four or condition indices
  above one outside the captured `np.ko` interface.
