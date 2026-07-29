# 0x16588 check_bppe

## Status

- Status: complete
- Confidence: verified.
- Size: `0x18` bytes, six ARM64 instructions.
- Recovered signature: `void check_bppe(void)`.

## Semantics

Private normal-pool BPPE diagnostic wrapper. It writes zero to `W0`, calls
`_check_abuf`, and has no semantic return value.

## Caller Context

No direct module xrefs were found. The collected runtime kallsyms marks this as
local text.

## Evidence

- Complete body at `0x16588` through `0x1659c`.
- Literal-zero argument setup and direct call to `_check_abuf @ 0x15e98` at
  `0x16594`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
