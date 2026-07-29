# 0x1afb8 zte_gephy_get_rx_stats

## Status

- Status: complete
- Confidence: verified three reads, selector writes, 16-bit values, log order,
  tail return, and no direct xrefs.
- Size: `0x90` bytes, 33 ARM64 instructions.
- Recovered signature: `int zte_gephy_get_rx_stats(u8 phy)`.

## Semantics

The helper reads MDIO register 20 and logs it as a RX CRC error count. It then
selects register-17 values with register-16 writes `0xffff9409` and
`0xffff940a`, logging each 16-bit read as RX count high and low halves. It
returns the result of the final low-half `printk` call.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY diagnostic path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or MDIO error handling exists. Counter
values are only emitted to the log and are not combined or returned directly.

## Evidence

- Complete ARM64 body at `0x1afb8` through `0x1b044`.
- Exact register reads, selector values, strings, `UXTH` conversions, and final
  tail `printk` return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1afb8` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Counter rollover, clear-on-read, and logging policy semantics.
