# 0x06174 mode_xgpon_syn_cfg

## Status

- Status: complete
- Confidence: verified CPU-133-only gate, single product-id load and predicate,
  all 49 ordered 32-bit stores, caller context, and semantic void ABI.
- Size: `0x278` bytes, 133 ARM64 instructions.
- Recovered signature: `void mode_xgpon_syn_cfg(void)`.

## Semantics

Logs `mode_xgpon_syn_cfg`, then applies the Mode-7 synchronous XGPON SerDes
profile only when `isCpuType_133() == 1`. A nonmatching CPU only logs and makes
no MMIO access.

The routine reads `product_vid` once after programming offsets `0x00..0x1c`.
It selects the product-dependent profile variant when any of these raw byte
tests is true:

- `(product_vid & 0x7f) == 0x06`
- `(product_vid & 0xfd) == 0xa4`
- `product_vid == 0x63` or `product_vid == 0x97`
- `(product_vid & 0xfb) == 0x01`

That variant writes `0x20=0x80000500`, `0x48=0x0f002b6a`, and
`0x80=0x10381038`; all other product IDs use `0x80001000`, `0x0f002b2a`, and
`0x103f103f` respectively. The remaining 46 words are fixed. All 49 MMIO
operations are direct ordered 32-bit stores at offsets `0x00..0xc0`.

The generic dispatcher arguments are not consumed before their argument
registers are overwritten by constants. The source preserves the raw product
tests rather than assigning unsupported product names.

## Return Semantics

The log result is discarded on the supported path, which retains a base-pointer
residual. A nonmatching CPU retains the CPU predicate result. Neither is a
semantic result, so the recovered ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller, invoking this profile for
case 7. `check_serdes_config @ 0x2a58` labels mode 7 `MODE_XGPON_SYN`.

## Evidence

- Complete ARM64 body at `0x6174` through `0x63e8`.
- CPU-133 gate at `0x6188`-`0x6190`.
- One byte load of `product_vid` at `0x61f8`; raw predicate construction at
  `0x61fc`-`0x6238`.
- Product-dependent stores at `0x624c`, `0x62a8`, and `0x6348`; all other
  profile stores run from `0x61a4` through `0x63e0`.
- The final `0xbc` and `0xc0` hardware writes at `0x63dc` and `0x63e0` are
  separate 32-bit stores despite Hex-Rays rendering them as a QWORD store.
- The only direct call is `serdes_mode_set` case 7 at `0x7d20`.
- IDA type at `0x6174` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
