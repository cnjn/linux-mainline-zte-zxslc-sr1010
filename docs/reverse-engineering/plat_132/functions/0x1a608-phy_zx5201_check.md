# 0x1a608 phy_zx5201_check

## Status

- Status: complete
- Confidence: verified double read, 16-bit truncation, gate bit, status-code
  mapping, packed return, and absence of direct code xrefs.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature: `int phy_zx5201_check(u8 phy)`.

## Semantics

The helper invokes `zx_mdio_read_ge_ext(phy, 26)` twice and discards the first
result. It truncates the second result to 16 bits. If bit six is clear it
returns `-1`. Otherwise it maps bits 8-9 to low return bits as follows: zero to
one, one to two, two to three, and three to seven. It ORs the value of bit seven
into return bit 10.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect PHY-dispatch path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs two
MDIO callback invocations in the caller's context.

## Evidence

- Complete ARM64 body at `0x1a608` through `0x1a684`.
- Two indirect calls through `zx_mdio_read_ge_ext` with identical `(phy, 26)`
  arguments; only the second result reaches `UXTH`.
- Exact bit-six gate, bits-8/9 mapping, and bit-seven shift by ten.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1a608` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of the returned low code and bit-10 status flag.
- Reason the first MDIO read is deliberately discarded.
