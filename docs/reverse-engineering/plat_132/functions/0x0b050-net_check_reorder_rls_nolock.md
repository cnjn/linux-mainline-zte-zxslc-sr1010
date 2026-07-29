# 0x0b050 net_check_reorder_rls_nolock

## Status

- Status: complete
- Confidence: verified slot iteration, count clamp, callback dispatch/context,
  index and counter updates, ops-table submission, callers, and lock context;
  context field labels are strong inference.
- Size: `0x150` bytes, 84 ARM64 instructions.
- Recovered signature: `void net_check_reorder_rls_nolock(void)`.

## Semantics

Initializes three local release-count slots to zero, then iterates while
`queue < rls_ring_num_max`. For each queue it obtains a raw release count through
IDM ops `+0x58` (`idm_get_reorder_rls`), clamps it to `0xfff`, and saves it in
the corresponding local slot.

For a nonzero count with an installed `idm_recycle_cb[queue]`, it builds a
stack-local context with these observed fields:

| Offset | Value copied before callback |
| --- | --- |
| `+0x0` | `tx_cmpl_ring_base[queue]` |
| `+0x8` | `rls_ring_size[queue]` |
| `+0xc` | `idm_rls_idx[queue]` |
| `+0x10` | clamped release count |

It invokes `callback(queue, &context)`, then advances the global release index
with `(count + index) & (ring_size - 1)` and increments `idm_rls_cnt[queue]`.
The post-callback updates do not occur when the callback is absent. Finally,
regardless of callback installation, it calls IDM ops `+0x60`
(`idm_rls_update`) with the three local counts.

The local count array and all slot-indexed global arrays have three elements, but
the binary has no bound check on `rls_ring_num_max`. Its data initializer is two;
`cpu_net_init` raises it to three only for CPU types 133 or 129.

## Caller Context

The only direct callers are `cpu_timer_func @ 0x0b7a4` and
`cpu_rls_poll @ 0x0b86c`. Both acquire `idm_lock_tx` before the call and release
it afterward. The helper itself has no lock acquisition.

`idm_get_reorder_rls @ 0x138f8` reads slot-specific IDM words for queues 0 and
1, and queue 2 only on CPU 133/129. `idm_rls_update @ 0x13740` writes the first
two counts as a packed word and the third count on those same CPU types.

## Concurrency and Ownership

- The callback sees a stack-local context and must not retain its address.
- Direct callers serialize release processing with `idm_lock_tx`.
- `register_woe*_recycle_handle` publishes callback slots without that lock, so
  registration can race callback dispatch and callback lifetime remains external.
- A zero ring size or non-power-of-two ring size is not guarded before the mask
  expression.

## Evidence

- Complete 84-instruction ARM64 body at `0xb050` through `0xb19c`.
- Exactly two direct callers, both with verified surrounding `idm_lock_tx`
  acquire/release sequences.
- Ops-table entries at `+0x58` and `+0x60` resolve to
  `idm_get_reorder_rls @ 0x138f8` and `idm_rls_update @ 0x13740`.
- `idm_init` initializes three adjacent completion-ring base values, and
  `cpu_net_init` initializes the matching normal/jumbo/extra ring-size values.
- The three `register_woe*_recycle_handle` setters identify callback slots at
  `0x27930`, `+0x8`, and `+0x10`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- External provider, lifecycle, and data-processing behavior for each recycle
  callback.
- Exact hardware meaning of the release counts and completion-ring contents.
