# 0x165b8 sipc_init

## Status

- Status: complete
- Confidence: verified MMIO write, return value, and all direct callers.
- Size: `0x18` bytes, six ARM64 instructions.
- Recovered signature: `int sipc_init(void)`.

## Semantics

Writes zero to the raw NPPT word at `nppt_base + 0x4000`, then returns zero.
There is no read-modify-write, lock, polling loop, barrier, logging, or error
path.

## Caller Context

- `zx_pon_probe @ 0x0580` calls it during NPPT bring-up and ignores the result.
- `nppt_init @ 0x11a50` calls it as its first initialization stage and ORs its
  zero result with later stage statuses.
- `sub_165A0 @ 0x165a0` falls through into this entry after its system-register
  prefix.

## Evidence

- Complete body at `0x165b8` through `0x165cc`.
- Direct xrefs at `0x0bec`, `0x11a68`, and fall-through from `0x165b4`.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
