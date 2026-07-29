# 0x01f98 serdes_prbs_err_ok

## Status

- Status: complete
- Confidence: verified SerDes offset/bit, RMW, log, return behavior, and
  absence of direct xrefs.
- Size: `0x30` bytes, 11 ARM64 instructions.
- Recovered signature: `int serdes_prbs_err_ok(void)`.

## Semantics

Sets bit 23 at SerDes offset `0x48` and returns the result of logging
`set 1 bit error ok`.

The symbol name could be read as a predicate, but the binary implements a
write-only command. The log suggests one-bit error injection; that hardware
interpretation remains a strong inference rather than a named register fact.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed PRBS test command.

## Evidence

- Complete ARM64 body at `0x1f98` through `0x1fc4`.
- Exact `LDR`/`ORR #0x800000`/`STR` sequence at offset `0x48`.
- Exact log string and tail return.
- IDA type at `0x1f98` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
