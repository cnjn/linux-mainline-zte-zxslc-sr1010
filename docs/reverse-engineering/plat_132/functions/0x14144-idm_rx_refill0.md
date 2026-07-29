# 0x14144 idm_rx_refill0

## Status

- Status: complete
- Confidence: verified for control flow, per-CPU staging arithmetic, and helper
  calls; strong inference for staging/status field labels.
- Size: `0x16c` bytes, 90 ARM64 instructions.
- Recovered signature:
  `int idm_rx_refill0(u32 old_buffer, int pool, int reuse)`.

## Role

This is the runtime RX refill operation exported through the IDM ops table. It
either reuses an existing buffer immediately or allocates a new pool-specific
buffer and stages its byte-swapped physical payload address for batch commit.

## Control Flow

```c
if (reuse) {
    idm_rx_refill_reuse(old_buffer, pool);
    return 0;
}

buffer = idm_alloc_buf(pool);
if (!buffer) {
    if (old_buffer)
        idm_rx_refill_reuse(old_buffer, pool);
    return -1;
}
```

Thus a failed allocation can still requeue a supplied old buffer, but the
function retains the failure result `-1`. A true reuse request always returns
zero after calling the reuse helper.

## New-Buffer Staging

For an allocated buffer, the function reads the current per-CPU `cpu_number`
through `__my_cpu_offset_0`, increments an `idm_status` counter, converts
`buffer + uBP_BUFFER_OFFSET + 64` to physical form, byte-swaps it, and stores
it in per-CPU staging data.

The observed staging layout is 264 bytes per CPU:

| Offset | Inferred contents |
| --- | --- |
| `0x000..0x07c` | 32 normal-pool 32-bit staged values |
| `0x080..0x0fc` | 32 jumbo-pool 32-bit staged values |
| `0x100` | normal staged-count |
| `0x104` | jumbo staged-count |

The address formula is `cpu * 66 + pool * 32 + slot` in 32-bit words, with
the count at `cpu * 66 + pool + 64`. Pool values are not range-checked. Once a
post-increment count exceeds 31, `idm_rx_refill_flush` is called.

The status counter index is:

```c
pool + 2 * cpu + (old_buffer >= buffer_data_phys ? 4 : 12)
```

where `buffer_data_phys` is the exact reserved-memory data-region boundary used
by the function. The hardware meaning of these status buckets is unknown.

## Reuse and Flush Evidence

- `idm_rx_refill_reuse @ 0x13d4c` locks `idm_refill_lock`, advances and wraps
  either normal or jumbo refill index, stores the byte-swapped old-buffer value
  directly into `rx_buf_ring` or `rx_jbuf_ring`, and releases with `STLRB`.
- `idm_rx_refill_flush @ 0x13c14` locks the same lock, drains both staging pools
  for the current CPU into the corresponding RX rings, zeroes their counts, and
  releases with `STLRB`.
- The jumbo staging drain's wrap comparison reads `uIDM_RX_NORMAL_BP_NUM`, not
  `uIDM_RX_JUMBO_BP_NUM`. This is directly verified in assembly and may be a
  vendor bug or a deliberate shared-size assumption; it is not normalized in
  the reconstruction.

## Concurrency and Ownership

New-buffer staging is per CPU and not locally locked in this function. Ring
commit/reuse is serialized by `idm_refill_lock`. The buffer returned by
`idm_alloc_buf` becomes owned by staged refill state once its physical address
is recorded; no local cleanup occurs after that point.

## Call Context

The only xref is an IDM ops-table entry at `0x26718`; no direct code caller in
this module constrains `old_buffer`, `pool`, or `reuse` inputs.

## Evidence

- Full 90-instruction ARM64 disassembly at `0x14144` through `0x142ac`.
- Direct decompilation and full assembly of `idm_rx_refill_reuse @ 0x13d4c` and
  `idm_rx_refill_flush @ 0x13c14`.
- Direct decompilation of `idm_alloc_buf @ 0x13df8` for allocation/fallback
  behavior.
- IDM ops-table xref at `0x26718`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- Valid pool values, status-counter meanings, and exact buffer ownership after
  external ops-table calls remain unresolved.
- Hardware doorbell behavior after staged ring writes is outside this wrapper
  and must be connected to later poll/refill paths.
