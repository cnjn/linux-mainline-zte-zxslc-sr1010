# 0x1a688 phy_zx5201_init

## Status

- Status: complete
- Confidence: verified all MDIO arguments, byte-truncated adjacent address,
  delay placement, read-modify-write, return source, and absence of direct xrefs.
- Size: `0x190` bytes, 98 ARM64 instructions.
- Recovered signature: `int phy_zx5201_init(u8 phy)`.

## Semantics

The function performs a fixed extended-MDIO initialization script using `phy`
and `(u8)(phy + 1)`. It writes primary-PHY pairs `(18, 0xffff8402)`,
`(29, 0x0355)`, `(16, 0xffffb62d)`, and `(17, 6)`; adjacent-PHY pairs
`(22, 0x0a07)`, `(27, 0x0800)`, `(18, 4)`, `(17, modified-register-21)`,
`(16, register-20)`, and `(18, 0x0204)`.

Each write except the intermediate adjacent `(18, 4)` is followed by
`__const_udelay(429500)`. The function reads adjacent registers 21 and 20;
it clears bits covered by `0x3e00` in register 21 then sets `0x2800` before
writing register 17. It returns the direct result of
`printk("rgmii phy init done\n")`.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect PHY-dispatch path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or error checking. The caller owns the
MDIO transport and waits for nine fixed delay intervals.

## Evidence

- Complete ARM64 body at `0x1a688` through `0x1a814`.
- Each `zx_mdio_write_ge_ext` register/value pair and both read arguments.
- `UXTB` after `phy + 1`, preserving byte-wrapped adjacent PHY addressing.
- Exact register-21 mask `0xffffc1ff`, OR value `0x2800`, delay constant
  `429500`, and tail `printk` return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1a688` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Vendor hardware meaning of the fixed extended-MDIO script.
- Whether an external PHY dispatcher observes the returned logging status.
