# 0x0b010 get_next_rxdesc

## Status

- Status: complete
- Confidence: verified base/index/stride computation, prefetch, producer wrap,
  callers, and configuration-global use; queue field labels are strong
  inference.
- Size: `0x40` bytes, 16 ARM64 instructions.
- Recovered signature: `uint8_t *get_next_rxdesc(zte_rx_queue_t *queue)`.

## Semantics

Returns the current RX descriptor address, then advances the queue producer:

```c
descriptor = queue->descriptor_base +
    (queue->producer << ((uNPPT_IDM_DESC_MODE + 5) & 31));
__builtin_prefetch(descriptor);
next = queue->producer + 1;
queue->producer = next >= queue->depth ? 0 : next;
return descriptor;
```

The explicit low-five-bit mask preserves ARM64 32-bit `LSL` shift behavior.
There is no null, depth, index, or descriptor-content validation. In the
captured initial state `uNPPT_IDM_DESC_MODE` is zero, yielding a 32-byte stride,
but the exported global is mutable and is not treated as a fixed constant.

## Caller Context

The only direct callers are `cpu_net_rx @ 0x0c5dc` and
`idm_net_rx @ 0x0bf6c`, both in NAPI RX processing. They receive queue records
through the IDM ops-table getter `idm_get_cpu_rx_qc @ 0x13088`. `idm_init`
initializes each 16-byte record with a descriptor base, zero producer, and
`uIDM_RX_QUEUE_DESC_DEPTH` depth.

## Globals and Side Effects

- Reads exported `uNPPT_IDM_DESC_MODE @ 0x292b8`; no MMIO access.
- Reads queue fields at `+0x0`, `+0x8`, and `+0xc`; writes only `+0x8`.
- Issues one non-faulting data prefetch for the returned descriptor address.

## Concurrency and Ownership

- No local lock, atomic operation, barrier, allocation, or ownership transfer.
- Correct producer progression requires callers to serialize access to a queue.

## Evidence

- Complete 16-instruction ARM64 body at `0xb010` through `0xb04c`.
- Exactly two direct IDA callers: `cpu_net_rx` and `idm_net_rx`.
- No callees; the only direct global reference is `uNPPT_IDM_DESC_MODE`.
- `idm_get_cpu_rx_qc` returns 16-byte `idm_rx_q` elements, and reconstructed
  `idm_init` initializes their matching base/producer/depth layout.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Hardware meaning and allowed runtime values of `uNPPT_IDM_DESC_MODE`.
- Per-queue serialization guarantees across NAPI/IRQ execution.
