# 0x0e524 dump_net_int_info

## Status

- Status: complete
- Confidence: verified input gate, context indexing, counter offsets, logging
  order, and no-side-effect behavior; semantic void ABI and names of the final
  three counters are strong inference from vendor log labels.
- Size: `0xa4` bytes, 35 ARM64 instructions.
- Recovered signature: `void dump_net_int_info(unsigned int source)`.

## Role

Diagnostic printer for one of the four contiguous CPU-net NAPI contexts. It
does not change the context, schedule work, acknowledge hardware, or return a
semantic status.

## Semantics

For unsigned inputs above 3, it prints `"invalid int %d\n"` and exits. For a
valid input, it prints `"net int %d\n"`, computes
`int_info + 0x1a0 * source`, and reads/prints five consecutive 32-bit words:

| Context offset | Vendor label | Recovered field |
| --- | --- | --- |
| `+0x18c` | `irq` | `irq_count` |
| `+0x190` | `irq_err` | `irq_err_count` |
| `+0x194` | `poll` | `poll_count` |
| `+0x198` | `rx int` | `rx_int_count` |
| `+0x19c` | `tx int` | `tx_int_count` |

`cpu_net_int` directly increments the first counter for every dispatch and the
second when NAPI scheduling was already prepared. The module has no direct
writer for the final three counters; their recovered names retain the vendor
print labels rather than asserting a producer.

The machine returns the final `printk` residual on the valid path and the only
`printk` residual on the invalid path. No caller establishes this as a return
contract, so the semantic ABI is void.

## Concurrency

All five values are ordinary independent reads. The routine provides no lock,
atomic snapshot, or retry, so its output can reflect concurrent counter updates.

## Evidence

- Complete ARM64 body at `0x0e524` through `0x0e5c4`.
- Unsigned `CMP W19,#3` and `B.LS` validation at `0x0e534`-`0x0e53c`.
- `MADD` with stride `0x1a0` at `0x0e568`.
- Five loads at `+0x18c`, `+0x190`, `+0x194`, `+0x198`, and `+0x19c` and their
  corresponding vendor strings.
- `cpu_net_int @ 0x0e188` directly corroborates the first two counter offsets.
- No direct IDA callers; runtime kallsyms marks the function module-local.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Producers and exact lifecycle meaning of `poll_count`, `rx_int_count`, and
  `tx_int_count` are outside this module's visible direct writes.
