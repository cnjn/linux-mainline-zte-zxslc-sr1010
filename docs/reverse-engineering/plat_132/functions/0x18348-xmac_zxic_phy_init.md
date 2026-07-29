# 0x18348 xmac_zxic_phy_init

## Status

- Status: complete
- Confidence: verified both XMAC-type gates, all three loop values, child-call
  order, ignored initializer status, and every callback-table store.
- Size: `0x114` bytes, 62 ARM64 instructions.
- Recovered signature: `void xmac_zxic_phy_init(void)`.

## Semantics

The function returns unless either `g_xmac0_type` or `g_xmac1_type` equals raw
value four. The first type test is short-circuited: XMAC1 is loaded only if
XMAC0 is not four.

When enabled, it loops indexes zero through two and passes PHY IDs four through
six to `phy_zxic051_port_exist`. For every nonzero result it calls
`phy_zxic051_para_init` but deliberately ignores that status, then writes these
callbacks at the same table index:

| Table | Callback |
| --- | --- |
| unnamed local table at `0x289a8` | `phy_zxic051_check` |
| `sg_xphy_enable_set` | `phy_zxic051_set_enable` |
| `sg_xphy_enable_get` | `phy_zxic051_get_enable` |
| `sg_xphy_linkstatus_get` | `phy_zxic051_get_linkstate` |
| `sg_xphy_linkmode_set` | `phy_zxic051_set_linkmode` |
| `sg_xphy_linkmode_get` | `phy_zxic051_get_linkmode` |
| `sg_xphy_loopback_set` | `phy_zxic051_set_loopback` |
| `sg_xphy_loopback_get` | `phy_zxic051_get_loopback` |

The reconstructed source calls the unexported first table
`xphy_check_callbacks`; this is a recovery-only descriptive name, not an
evidenced original symbol. The existing port predicate currently accepts only
PHY ID five when its port is configured, but this initializer still performs all
three exact loop iterations. No local lock, barrier, null check, or rollback is
present.

## Caller Context

`xmac_init @ 0x18460` is the sole direct in-module caller. It invokes this
function last in its PHY-helper sequence after excluding raw PHY type nine and
discards the residual return register. The function is local text, so the
recovered interface is semantic `void`.

## Evidence

- Complete ARM64 body at `0x18348` through `0x18458`.
- Type comparisons at `0x18354` through `0x1839c` establish the raw-value-four
  gate and short-circuit XMAC1 access.
- Loop control at `0x183a4` through `0x183c4` establishes indexes 0..2 and PHY
  bytes 4..6.
- Direct calls to `phy_zxic051_port_exist` and `phy_zxic051_para_init` at
  `0x183c0` and `0x183cc`; `CBZ W0` proves the existence gate.
- All eight callback stores at `0x183d0` through `0x18440`, including the
  separate unexported table at `0x289a8` and seven runtime-exported tables.
- Sole caller xref at `xmac_init + 0xb4`; runtime kallsyms confirms local text
  and the exported `sg_xphy_*` table names.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
