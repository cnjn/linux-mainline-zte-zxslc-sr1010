# 0x0eb50 upload_write_proc

## Status

- Status: complete
- Confidence: verified user-range/copy/parse/callback/return behavior and proc
  operations-table xref; inlined user-range-check label is inferred.
- Size: `0xec` bytes, 59 ARM64 instructions.
- Recovered signature:
  `long upload_write_proc(void *file, const char __user *buffer, unsigned long count)`.

## Semantics

This is the write handler referenced by the upload proc operations data. It
saves `count`, sets only `input[0] = 0`, runs an inlined vendor user-range check
for **eight** bytes, then unconditionally copies exactly eight bytes from the
user pointer. It returns `-1` if range validation or copy reports failure.

When copied byte zero is nonzero, it passes the un-NUL-terminated local 8-byte
buffer to `simple_strtoul(..., 10)`, truncates the result to an unsigned byte,
including a zero first byte, the function returns the original supplied count.

It does not use `count` to limit the access or copy. Therefore a write shorter
than eight bytes still requests an eight-byte user read, and an eight-byte input
without an earlier NUL can make `simple_strtoul` read past the local array. These
are observed binary behaviors, not reconstructed defensive-policy omissions.

## Caller Context

The sole xref is a data reference from the upload proc operations object at
`0x26560`; no direct code caller exists because the proc filesystem dispatches
the callback.

## Concurrency and Ownership

- No local lock, allocation, or skb ownership behavior.
- Reads user memory and delegates shared upload-state serialization to
  `net_upload_fun`.

## Evidence

- Complete 59-instruction ARM64 disassembly at `0xeb50` through `0xec38`.
- Direct decompilation of inline range check, exact eight-byte copy, conversion,
  and return paths.
- Data xref from upload proc operations table and direct `net_upload_fun` call.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact vendor user-range-check macro/state fields and proc operations ABI.
- Whether an external policy prevents short or non-NUL-terminated writes.
