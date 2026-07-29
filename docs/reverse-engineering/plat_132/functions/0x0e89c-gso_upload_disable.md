# 0x0e89c gso_upload_disable

## Status

- Status: complete
- Confidence: verified mode-dependent loop, nbuf mutation/free behavior,
  count/reset behavior, and both callers; raw nbuf field labels are inferred.
- Size: `0xc8` bytes, 50 ARM64 instructions.
- Recovered signature: `void gso_upload_disable(unsigned int release_buffers)`.

## Semantics

The function loops from zero while `index < gso_buf_cnt`, re-reading the global
count on every iteration. A null `gso_nbuf_pool[index]` is skipped entirely,
including its `s_gso_last_hlen[index]` reset.

For a non-null entry:

- `release_buffers != 0`: clear nbuf byte `+0x2c` bit 1, call
  `cpu_net_free_nbuf`, and clear the pool slot.
- `release_buffers == 0`: zero the buffer at nbuf `+0x18` for
  `uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE - uBP_BUFFER_OFFSET - 64` bytes.

Both paths clear `s_gso_last_hlen[index]`. When `release_buffers` is nonzero it
sets `gso_buf_cnt = 0` after the loop; it does not reset `gso_buf_idx`.

## Important Failure Edge

`gso_upload_enable` writes pool slots before publishing `gso_buf_cnt = 64`. If
allocation fails partway through, its `gso_upload_disable(1)` call sees a zero
count, executes zero loop iterations, and leaves all previously allocated nbufs
and pointers intact. This is a verified partial-allocation leak.

## Caller Context

- `net_upload_fun @ 0x0ea40` calls `gso_upload_disable(0)` whenever a zero
  upload request leaves the reference count at zero; this resets non-null
  buffers without freeing them.
- `gso_upload_enable @ 0x0e964` calls `gso_upload_disable(1)` after allocation
  failure, but its uncommitted count makes the cleanup ineffective.

## Concurrency and Ownership

- No local lock; callers are responsible for serialization.
- Release mode frees pool-owned nbufs only when count makes them visible.

## Evidence

- Complete 50-instruction ARM64 disassembly at `0xe89c` through `0xe960`.
- Two direct callers, raw pool/length/count field accesses, and exact mode
  branch instruction sequence.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original nbuf ABI and whether any external recovery path handles leaked
  partial allocation state.
