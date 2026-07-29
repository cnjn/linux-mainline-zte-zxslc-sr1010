# 0x17da0 xmac_init_by_work_mode

## Status

- Status: complete
- Confidence: verified byte truncation, pre-switch initialization, all ten mode
  dispatches, post-configuration sequence, success/failure state handling,
  raw SOPC write, return values, and both callers.
- Size: `0x184` bytes, 95 ARM64 instructions.
- Recovered signature: `int xmac_init_by_work_mode(u8 xmac, u32 work_mode)`.

## Semantics

The incoming XMAC selector is truncated to a byte. Before validating
`work_mode`, it always calls `xpcs_init(xmac)` followed by `xmac_reset(xmac)`.
Modes zero through nine dispatch as follows:

| Mode | Setter |
| --- | --- |
| 0 | `xmac_10gbase_r_conf(xmac)` |
| 1 | `xmac_5gbase_r_conf(xmac)` |
| 2 | `xmac_1gbase_x_conf(xmac)` |
| 3 | `xmac_sgmii_conf(xmac, 0, 3, 1)` |
| 4 | `xmac_2pt5gbase_x_conf(xmac)` |
| 5 | `xmac_10g_usxgmii_auto_conf(xmac)` |
| 6 | `xmac_5g_usxgmii_auto_conf(xmac)` |
| 7 | `xmac_2pt5g_usxgmii_auto_conf(xmac)` |
| 8 | `xmac_hsgmii_conf(xmac, 1)` |
| 9 | `xmac_hsgmii_conf(xmac, 0)` |

Any other signed or unsigned mode logs `unspport work mode` and returns `-1`
after only the XPCS initialization and reset.

For every valid mode, including a setter failure, the function then:

1. Calls `xmac_set_duplex_mode(xmac, 1)`.
2. Calls `xmac_set_sopc_duplex_mode(xmac, 1)`.
3. Calls `__const_udelay(0x418958)`.
4. Writes one to raw `nppt_base + 0x34000 + 4 * ((xmac + 0xb0) & 0x1ff)`.
5. Calls `xmac_tx_rx_enable(xmac)`.

If the setter status is nonzero, it logs and returns that status without writing
`sg_xmac_work_mode[xmac]`. On zero status, it stores the full 32-bit requested
mode into that array, logs success, and returns zero. No bounds check protects
the byte-sized XMAC index or the work-mode array.

## Caller Context

`xmac_init @ 0x18460` has exactly two direct call sites: enabled XMAC0 setup
uses selector zero and XMAC1 setup uses selector one. Its caller ORs both
returned statuses when both gates are enabled.

## Concurrency and Ownership

No local lock, allocation, or cleanup. It directly writes the raw SOPC register
and delegates all XPCS/XMAC configuration and enable semantics to its callees.
The work-mode cache is written only after a successful setter result.

## Evidence

- Complete 95-instruction ARM64 body at `0x17da0` through `0x17f20`.
- `UXTB W19, W0`, ten-entry jump table, and raw setter argument registers.
- Shared post-setter path proves duplex calls, delay literal, SOPC formula, and
  TX/RX enable run even after a nonzero setter result.
- Success-only cache store, invalid-mode `0xffffffff` return, and two caller
  xrefs from `xmac_init`.
- Runtime dmesg shows successful work modes five and four on CPU 133.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of the raw SOPC register and delay calibration value.
- Detailed register-level effects and failure contracts of each mode setter.
- Valid XMAC selector range and lifecycle of `sg_xmac_work_mode`.
