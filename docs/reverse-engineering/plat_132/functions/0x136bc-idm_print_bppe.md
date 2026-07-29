# 0x136bc idm_print_bppe

## Status

- Status: complete
- Confidence: verified single 32-bit argument formatting and exported-interface
  status.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void idm_print_bppe(u32 address)`.

## Semantics

Prints the one input word using the literal format `addr %x\n`. It reads no
global/MMIO state and takes no lock. The residual `printk` result in `W0` is not
an evidenced status return.

## Caller Context

There are no direct module code callers. `__ksymtab_idm_print_bppe @ 0x1c7b0`
exports the diagnostic helper.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
