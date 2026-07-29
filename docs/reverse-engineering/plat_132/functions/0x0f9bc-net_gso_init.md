# 0x0f9bc net_gso_init

## Status

- Status: complete
- Confidence: verified global stores, proc creation/error paths, callback
  publication, caller, and return use; proc-operations type is inferred.
- Size: `0xa4` bytes, 35 ARM64 instructions.
- Recovered signature: `void net_gso_init(void)`.

## Semantics

The function sets `g_upload_driver_en` to 1, creates `/proc/upload_ctl`, and
when that succeeds attempts to create `/proc/upload_ctl/upload` with mode 128
and the operations object at `upload_test_fops`.

Failure to create the directory logs `"Can't create /proc/upload_ctl"` only if
`__printk_ratelimit("upload_test_proc_init")` permits it. Failure to create the
file is handled independently with the same ratelimit and the literal vendor
message `"create porc/upload_ctl/upload!"`. No proc cleanup or global rollback
occurs on either failure.

After all proc branches, it stores `net_upload_fun` in `upload_hook`. The
decompiler's apparent function-pointer return is residual register state; the
sole caller ignores it.

## Caller Context

`cpu_net_init @ 0x0e220` calls it immediately after `net_gro_init` during CPU
network setup.

## Concurrency and Ownership

- No local lock, allocation ownership tracking, or direct MMIO.
- A successful proc directory/file is not removed in this function's error
  paths. Callback publication occurs even when proc creation fails.

## Evidence

- Complete 35-instruction ARM64 disassembly at `0xf9bc` through `0xfa5c`.
- Sole direct caller xref from `cpu_net_init` at `0xe460`.
- Exact proc/ratelimit/global/callback argument setup and branches.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original proc-operations structure ABI and teardown path.
- Signature, registration, and external invocation contract of `net_upload_fun`.
