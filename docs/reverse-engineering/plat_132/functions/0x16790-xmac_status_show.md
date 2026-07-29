# 0x16790 xmac_status_show

## Status

- Status: complete
- Confidence: verified local table contents/size, unchecked index conversion,
  global reads, output order, stack protector, and export status.
- Size: `0x1ac` bytes, 102 ARM64 instructions.
- Recovered signature: `void xmac_status_show(void)`.

## Semantics

Builds a zeroed 11 x 64-byte local table. Slots 0 through 9 contain, in order:
`10G BASE-R`, `5G BASE-R`, `1000BASE-X`, `SGMII`, `2.5G BASE-X`,
`10G USXGMII`, `5G USXGMII`, `2.5G USXGMII`, `HSGMII`, and `NONE`; slot 10
remains zeroed.

It prints two sections using `sg_xmac_work_mode[0/1]` as unvalidated unsigned
32-bit indices into that local 64-byte-stride table, followed by the respective
`g_xmac_work_in_auto[0/1]` byte. It reads no XMAC MMIO despite its name and
performs no state mutation other than stack-local construction.

## Caller Context

No direct module xrefs were found. Runtime kallsyms marks this function as
exported global text; its consumers are therefore outside the module image.

## Evidence

- Complete ARM64 body at `0x16790` through `0x16938`.
- `memset` of exactly `0x2c0` bytes and string copies at 64-byte strides.
- Direct global reads at `0x168bc`, `0x168d8`, `0x168f0`, and `0x16904`.
- Literal strings at `0x1e645` through `0x1e885` and output formats at
  `0x24311` through `0x2434a`.
- Runtime `__ksymtab_xmac_status_show` and global-text kallsyms entries.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
