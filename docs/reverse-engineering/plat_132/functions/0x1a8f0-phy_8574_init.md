# 0x1a8f0 phy_8574_init

## Status

- Status: complete
- Confidence: verified one-time guard, initial poll ordering/bound, all MDIO
  page/register operations, delay placement, tail return, and no direct xrefs.
- Size: `0x2c0` bytes, 161 ARM64 instructions.
- Recovered signature: `int phy_8574_init(u8 phy)`.

## Semantics

The first call observed by the module-wide byte guard logs setup, selects page
16, writes register 18 with `0xffff80f0`, then polls register-18 bit 15. The
first read occurs before the loop; each busy iteration decrements a 16-bit
counter initialized to 1002 and reads again without delay. When the counter
reaches zero it logs a timeout, then sets the guard anyway.

Every call subsequently executes a fixed extended-MDIO page/register script. It
uses page 3/register 16 value `0x01a0`, page 0/register 0 value `0xffff9040`,
page 16/register 25 masked with `0xfffe`, page 2/register 17 masked with
`0xc3ff`, and page 0/register 29 masked with `0xfff0`. It writes register 18
value eight, returns to page 3 for another register-16 write, then page zero.
Each listed page/register operation has `__const_udelay(429500)` exactly where
the binary does, except the immediate register-18/page-3 pair. It returns the
final `printk("phy init: phy_id = 0x%x\n", phy)` status.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect PHY-dispatch path; direct xrefs cannot establish that.

## Concurrency and Ownership

The shared one-time guard is an unprotected byte. Concurrent first callers can
both enter the initial sequence. The timeout has no error return and permanently
suppresses the first-call sequence thereafter.

## Evidence

- Complete ARM64 body at `0x1a8f0` through `0x1abac`.
- Guard byte at `0x291f8` has no references outside this function.
- Exact initial poll value/mask/counter, all page/register values, RMW masks,
  13 delay calls, and tail `printk` return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1a8f0` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware purpose of each vendor page/register setting.
- Whether the shared guard reflects a shared four-port PHY package.
