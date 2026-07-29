# 0x131a4 get_last_buffer_idx

## Status

- Status: complete
- Confidence: verified selector branches, all three 32-bit reads, invalid
  selector return, and exported-interface status.
- Size: `0x4c` bytes, 19 ARM64 instructions.
- Recovered signature: `u32 get_last_buffer_idx(u32 pool)`.

## Semantics

Returns `last_normal_idx` for pool zero, `last_jumbo_idx` for pool one, and
`last_extral_idx` for pool two. Any other pool value returns zero. It does not
lock or validate the selected global.

## Caller Context

No direct module code caller exists; `__ksymtab_get_last_buffer_idx @ 0x1c720`
exports the query API.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- External users and the lifecycle of these three index globals are not present
  in this module's code xrefs.
