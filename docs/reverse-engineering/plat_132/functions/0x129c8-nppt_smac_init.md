# 0x129c8 nppt_smac_init

## Status

- Status: complete
- Confidence: verified initialization order, raw MMIO offsets/values, callback
  array layout, XMAC mode-selection table, worker launch, fixed zero return,
  direct caller, and runtime sequence. Hardware field names and PHY-type labels
  remain strong inferences or unknown.
- Size: `0x5c0` bytes, 353 ARM64 instructions.
- Recovered signature: `int nppt_smac_init(void)`.

## Semantics

The function always returns zero. It does not propagate a status from PHY
callbacks, SMAC reset, SOPC enable, XMAC setup, auto-negotiation setup, or the
PHY polling-thread creation helper.

It sets `g_smac_max_index` to two only when `isCpuType_129() == 1`; all other
CPU types receive three. It initializes all seven slots of these raw arrays:

| Array | Initialized value |
| --- | --- |
| `uni_phy_stat[0..6]` | `-2` |
| `sg_smac_check_phy[0..6]` | null |
| `sg_smac_init_phy[0..6]` | null |
| `sg_smac_set_phy_enable[0..6]` | null |
| `sg_smac_get_phy_enable[0..6]` | null |

It then writes `xmac_phy_id[1] = 1`, enumerates signed logical-port indices
from zero through `get_capability_for_product() - 1`, and installs the four
GEPHY callbacks only when the returned switch/UNI port is an unsigned value at
most three. Negative values and all values above three are logged but leave the
callback slots null.

For each MAC index from zero through the current `g_smac_max_index`, the
function optionally invokes `sg_smac_init_phy[mac](uni_phy[mac])`, clears raw
UNI-mode bits through `nppt_smac_set_uni_mode(mac, 0)`, calls
`smac_reset(1U << mac)`, and programs this raw register block:

| Raw offset from `nppt_base` | Value/action |
| --- | --- |
| `(mac + 1) * 0x40000 + 0x0` | write `0x00ba2203`, reset, then write `0x00ba2200` |
| `(mac + 1) * 0x40000 + 0x4` | write `0x00003fff` |
| `(mac + 1) * 0x40000 + 0x8` | write `0x80000001` |
| `(mac + 1) * 0x40000 + 0xb00` | read-modify-write OR `0x200` |
| `(mac + 1) * 0x40000 + 0xe0` | write `0x00011200` |
| `0x343f0` | set/clear the MAC bit according to bit 13 of the block word |

The per-MAC loop calls `isCpuType_133()` immediately before the `+0xb00`
update, but the machine code does not consume its result. `sopc_send_enable`
receives the MAC index after the raw `0x343f0` update.

On CPU 129 or 133 only, `xmac_phy_type()` selects the two work-mode arguments
for `xmac_init` below. `Is_279051_phy` affects the raw `g_xmac*_type` values and
the type-6 first work mode as shown.

| PHY type | `xmac_init` modes `(0, 1)` | Type-state updates | Skip USXGMII auto-negotiation |
| --- | --- | --- | --- |
| 0 | `(4, 0)` | `g_xmac0_type = 4` or `1` | yes |
| 1 | `(0, 4)` | `g_xmac1_type = 4` or `1` | yes |
| 2 | `(4, 4)` | both type values `4` or `1` | yes |
| 3 | `(3, 0)` | `g_xmac0_type = 1` | yes |
| 5 | `(5, 4)` | `g_xmac1_type = 4` or `1` | no |
| 6 | `(4, 4)` if 279051, otherwise `(6, 4)` | `g_xmac1_type = 4` or `1` | no |
| 7 | `(5, 5)` | `g_xmac1_type = 1` | no |
| 8 | `(2, 3)` | `g_xmac1_type = 1` | no |
| 9 | `(5, 2)` | none | no |
| 11 | `(2, 2)` | none | no |
| other | `(0, 0)` | none | yes |

After `xmac_init`, it calls
`xpcs_auto_negotiation_conf_in_usxgmii_mode(0, 0)` exactly when the table does
not skip it and XMAC0 work mode is five. Finally it calls `smac_thread_init`
and returns zero.

## Caller Context

`nppt_init @ 0x11a50` is the sole direct in-module caller. It invokes this
function after `sipc_init` and `greg_init`, before `idm_init`; the fixed zero
return means this stage cannot contribute nonzero bits to `nppt_init`'s status
OR.

## Concurrency and Ownership

The function has no local lock, allocation, or unwind. It establishes callback
arrays before calling `smac_thread_init`, which creates the worker that polls
all seven MAC indexes. The raw globals and MMIO writes are not locally
synchronized; initialization ordering is the only observed protection.

## Evidence

- Complete 353-instruction ARM64 body at `0x129c8` through `0x12f84`.
- Direct caller xref from `nppt_init @ 0x11a50`.
- Direct disassembly of `nppt_smac_set_uni_mode`, `smac_reset`,
  `sopc_send_enable`, `xmac_init`, CPU-type predicates, and the PHY worker.
- Callback implementation and later `check_phy @ 0x126e4` use confirm the
  seven-slot status/callback layout and `uni_phy[mac]` initializer argument.
- Runtime dmesg records the four MAC 0-3 reset/configuration/SOPC sequences,
  XMAC work modes five/four, and successful PHY-worker creation on CPU 133.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meanings of all raw NPPT register fields and XMAC PHY type values.
- Exact producer and consumer contracts for the callback arrays.
- Why the per-MAC loop calls `isCpuType_133()` while discarding its result.
- Thread lifecycle and cleanup behavior after `smac_thread_init` succeeds or
  fails.
