# 0x1150c net_gro_init

## Status

- Status: complete
- Confidence: verified.
- Size: `0x14` bytes, 4 ARM64 instructions.
- Recovered signature: `void net_gro_init(void)`.

## Semantics

The function stores the address of `lower_net_smb_test_config` in the global
`pp_smb_test_config` callback slot. It has no input, allocation, lock, error
path, GRO-table initialization, or direct MMIO access.

The decompiler's apparent function-pointer return is residual register state;
the caller ignores it and the ARM64 body returns immediately after the store.

## Caller Context

`cpu_net_init @ 0x0e220` calls this helper after `testftp_init` and before
`net_gso_init` while constructing CPU networking state.

## Evidence

- Complete four-instruction ARM64 disassembly at `0x1150c` through `0x1151c`.
- Sole caller xref from `cpu_net_init` at `0x0e45c`.
- Direct global/function-address store operands.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Callback registration, invocation sites, and the exact argument/return
  contract of `lower_net_smb_test_config` remain pending.
