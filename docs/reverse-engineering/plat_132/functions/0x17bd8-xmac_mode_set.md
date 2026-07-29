# 0x17bd8 xmac_mode_set

## Status

- Status: complete
- Confidence: verified outer/switch validation, byte truncations, speed lookup
  table, every mode-to-setter dispatch, speed-selection tail, direct returns,
  and all three caller xrefs.
- Size: `0x160` bytes, 85 ARM64 instructions.
- Recovered signature:
  `int xmac_mode_set(u8 xmac, u32 pcs_mode, u8 input_speed, u8 config_value)`.

## Semantics

The function rejects `pcs_mode > 12` with the outer-PHY error message and
returns `-1`. Values 10 through 12 reach a separate switch-default message
(`xmac doesn't support this pcs mode`) and also return `-1`.

It truncates the XMAC selector, `input_speed`, and `config_value` to bytes.
Before dispatch, it computes `index = (u8)(input_speed - 1)`. If `index <= 5`,
it obtains the initial speed value from the six-byte read-only table:

| Input speed | Table value |
| --- | --- |
| 1 | 7 |
| 2 | 4 |
| 3 | 3 |
| 4 | 2 |
| 5 | 5 |
| 6 | 0 |

All other input-speed bytes retain initial speed value one. The PCS-mode
dispatch is:

| PCS mode | Setter | `xmac_set_speed_sel` value |
| --- | --- | --- |
| 0 | `xmac_10gbase_r_conf` | 0 |
| 1 | `xmac_5gbase_r_conf` | 5 |
| 2 | `xmac_1gbase_x_conf` | 3 |
| 3 | `xmac_2pt5gbase_x_conf` | 2 |
| 4 | `xmac_hsgmii_conf(xmac, 0)` | no local call |
| 5 | `xmac_sgmii_conf(xmac, 0, table_or_default, config_value)` | table/default |
| 6 | `xmac_sgmii_conf(xmac, 1, 3, 1)` | 1 |
| 7 | `xmac_10g_usxgmii_auto_conf` | 1 |
| 8 | `xmac_5g_usxgmii_auto_conf` | 1 |
| 9 | `xmac_2pt5g_usxgmii_auto_conf` | 1 |

Every successful dispatch except PCS mode four calls `xmac_set_speed_sel` after
the setter, even when that setter returned an error. The setter's raw status is
returned unchanged; the speed-selection helper's return register is discarded.

## Caller Context

Three direct call sites occur in `phy_zxic051_check @ 0x1c0c0`. This function
is therefore the verified PHY-driven runtime XMAC mode-selection boundary.

## Concurrency and Ownership

No local lock, allocation, or cleanup. All hardware effects are delegated to
the selected setter and speed-selection helper. No local persistent state is
written.

## Evidence

- Complete 85-instruction ARM64 body at `0x17bd8` through `0x17d34`.
- Outer unsigned limit, six-byte table at `0x1e8c8`, and ten-entry switch table.
- Exact setter argument registers, mode-four direct return, and post-setter
  speed-selection paths.
- All three caller xrefs from `phy_zxic051_check`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Meaning of PCS modes 10 through 12 and why they use a different diagnostic.
- Hardware meaning of the six-byte speed lookup table and `config_value`.
- Whether all three PHY-driven call sites are serialized against concurrent
  XMAC mode changes.
