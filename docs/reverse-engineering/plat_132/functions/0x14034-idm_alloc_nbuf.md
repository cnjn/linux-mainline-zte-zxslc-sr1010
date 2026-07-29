# 0x14034 idm_alloc_nbuf

## Status

- Status: complete
- Confidence: verified pool-zero allocation, reserved-boundary status branch,
  per-CPU counter indices, all raw header writes, null return, and ops-table
  role.
- Size: `0x110` bytes, 68 ARM64 instructions.
- Recovered signature: `void *idm_alloc_nbuf(void)`.

## Semantics

Calls `idm_alloc_buf(0)` and returns null unchanged on failure. For a successful
allocation, recomputes the IDM reserved data-region boundary, converts it to a
linear virtual address, then increments one raw `idm_status` lane for the
current CPU:

```c
buffer < data_boundary ? idm_status[2 * cpu + 12]
                       : idm_status[2 * cpu + 4]
```

It initializes only raw header fields: clears eight bytes at `+0x00`, clears the
16-bit field at `+0x28`, writes `buffer + 0x40` at `+0x10`, clears the 32-bit
field at `+0x2c`, and writes `buffer + uBP_BUFFER_OFFSET + 64` at `+0x18`.
No full-object initialization, cache operation, or ownership validation occurs.

## Caller Context

The only xref is an IDM ops-table entry at `0x26700` (`+0x28`); external CPU-net
consumers invoke this normal-buffer allocator callback.

## Evidence

- Complete ARM64 body at `0x14034` through `0x14140`.
- Direct `idm_alloc_buf(0)` call at `0x14040`.
- Exact reserved-boundary arithmetic at `0x14050` through `0x140dc`.
- Per-CPU status increment and raw header stores at `0x140e4` through `0x14138`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- The raw header field names and the distinct meaning of status lanes `+4` and
  `+12` remain inferred from use, not vendor type metadata.
