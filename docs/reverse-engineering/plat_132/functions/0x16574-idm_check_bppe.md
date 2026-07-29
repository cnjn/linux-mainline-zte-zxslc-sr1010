# 0x16574 idm_check_bppe

## Status

- Status: complete
- Confidence: verified.
- Size: `0x14` bytes, five ARM64 instructions.
- Recovered signature: `void idm_check_bppe(uint8_t pool_selector)`.

## Semantics

Forwards the input byte unchanged to private `_check_abuf`. The private checker
uses only the byte's low bit to select normal or jumbo BPPE diagnostics. The
callee's residual register values are not a semantic return contract.

## Caller Context

No direct module code xrefs were found. It is exported, as confirmed by the
collected runtime kallsyms entry, so it forms the external BPPE diagnostic API.

## Evidence

- Complete body at `0x16574` through `0x16584`.
- Direct branch-and-link to `_check_abuf @ 0x15e98` at `0x1657c`.
- Runtime kallsyms marks `idm_check_bppe` as global text.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
