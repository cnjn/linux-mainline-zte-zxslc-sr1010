# 0x0184c serdes_unlock

## Status

- Status: complete
- Confidence: verified both SerDes offsets/masks, void action ABI, and absence
  of direct code xrefs.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature: `void serdes_unlock(void)`.

## Semantics

Clears bits 13-14 at PON SerDes offset `0x90`, then clears bit 15 at offset
`0x40`.

## Caller Context

No direct code xrefs target this entry; it may be an external SerDes action API.

## Evidence

- Complete ARM64 body at `0x184c` through `0x186c`.
- Exact offsets, masks, and stores.
- X0 only retains the base-address temporary; no semantic result is formed.
- IDA type at `0x184c` updated to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
