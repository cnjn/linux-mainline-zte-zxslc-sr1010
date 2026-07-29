# 0x0fa60 sub_FA60

## Status

- Status: complete
- Confidence: verified raw PAN writes, physical fall-through, argument
  preservation, and zero direct xrefs; purpose of the PAN toggle is unknown.
- Size: `0x8` bytes, 2 ARM64 instructions plus fall-through into
  `testftp_net_report @ 0x0fa68`.
- Recovered signature:
  `int sub_FA60(const void *data, const u8 *metadata, u32 received_length)`.

## Semantics

The entry executes `MSR PAN,#0` followed immediately by `MSR PAN,#1`. It has no
return instruction, so control falls directly into `testftp_net_report` with
the incoming `x0`, `x1`, and `w2` arguments unchanged; its returned value is
therefore that report function's result.

No direct in-module xrefs target this entry. The short privileged-state fragment
is preserved literally instead of being assigned a standard access-control role.

## Evidence

- Complete two-instruction body at `0x0fa60` through `0x0fa64`.
- Function boundary ends at `0x0fa68`, which is the report function entry.
- Both instructions write immediate PAN states and no argument register.
- Zero direct IDA xrefs to the entry.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Why this unreferenced wrapper toggles PAN before the report path.
