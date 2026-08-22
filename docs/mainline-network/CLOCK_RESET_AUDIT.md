# ZX279133 Selected-Path Clock and Reset Audit

## Scope

This is the Phase 1.1 ownership audit for the first SR1010 WAN path:

```text
ZX279051 -> PON SerDes -> XPCS1 -> XMAC1 -> NPPT/IDM
```

It covers only registers required before or during 1G/2.5G link-layer bring-up.
Full NPPT packet-processing clocks are classified here but implemented later
with the Ethernet driver.

## Ownership Decision

| Physical range | Owner | Policy |
| --- | --- | --- |
| TOPCRM `0x10e10000..0x10e1005f` | ZX279133 TOPCRM CCF driver | Existing clock mux, divider, and gate provider; no second MMIO owner |
| TOPCRM `0x10e10060..0x10e10063` | `top_reset` reset controller | Active-low reset bits |
| TOPCRM `0x10e10070..0x10e10077` | `local_reset` reset controller | Active-low local reset bits; selected path consumes bits 0 and 1 |
| TOPCRM `0x10e100c0..0x10e100c7` | PON SerDes PHY mode resource | Non-overlapping mode-specific PLL words; not exposed as a generic clock until parent/rate semantics are known |
| sysctrl `0x10e00078..0x10e0007f` | Future NPPT/IDM initialization | One-time CCI/coherency setup, not a CCF clock |
| NPPT internal reset/route words | Future NPPT Ethernet driver | Internal reset and cross-block routing, not reset-controller or CCF resources |
| PON `0x17000080` route word | PON SerDes integration owned by the first Ethernet path | Route selection only; not a clock |

The TOPCRM node is a syscon, but the current clock driver performs raw MMIO
access protected by its private spinlock. A second driver must not write a field
that a writable CCF clock can modify concurrently.

For the first clean implementation, the existing PON/NPPT muxes remain
read-only. The PON SerDes PHY may perform bounded, one-time syscon updates only
to fields not written by current CCF operations: TOPCRM `0x0c` bits 8-9 and
`0x10` bits 4-5. If those fields later become writable CCF objects, their
updates must move into one shared owner.

## TOPCRM Field Matrix

### Offset `0x04`: CPU and CCI Mux Control

| Bits | Recovered operation | Runtime | Current CCF | Decision |
| --- | --- | --- | --- | --- |
| 4-6 | CPU133 `pon_soc_pon_cci_aclk_init()` ORs `0x70` | Vendor `cci_aclk` reports 500 MHz | Read-only `cci_aclk` mux; value 7 selects `clk500m` | Existing CCF representation is consistent; do not write it from the network driver |

This clock is shared system infrastructure, not a private PON gate. The first
network driver consumes the CCI clock only if a real hardware clock dependency
is demonstrated; it must not reselect the global CCI parent.

### Offset `0x0c`: Shared PON/NPPT Mux Control

| Bits | Vendor operation on CPU133 | Captured transition | Current model | Decision |
| --- | --- | --- | --- | --- |
| 0-2 | TM ACLK helper ORs `0x7` | Post-init field is 7 | Unmodeled | Defer to NPPT Ethernet clocks; parent encoding is not yet reconciled |
| 4-6 | PON core helper ORs `0x70` | Post-init field is 7 | Unmodeled | Not required for isolated SerDes programming; defer |
| 8 | `pon_pll_cfg()` clears bit 8 for Ethernet modes 8-16 | `1 -> 0` | Unmodeled | PON SerDes mode-select field; one-time PHY syscon update |
| 9 | `pon_pll_cfg()` sets bit 9 for every supported mode | `0 -> 1` | Unmodeled | PON SerDes PLL/config enable field; one-time PHY syscon update |
| 16 | RGMII helper controls the bit | Not sampled as changed | Unmodeled | Not selected; leave untouched |
| 20-22 | WOE helper ORs `0x00700000` | Post-init field is 7 | Unmodeled | Defer until minimum NPPT forwarding is implemented |
| 24-26 | NPPT helper writes value 6 on CPU133 | `7 -> 6` | Read-only `pon_nppt_wclk_mux`; value 6 is 416.666 MHz | Existing parent table and runtime agree; preserve read-only firmware selection initially |

The complete observed word changes from `0x07711177` to `0x06711277`. The
vendor helper sequence accounts for the changed mux field and bits 8-9.

### Offset `0x10`: PON PLL Source/Profile Select

For Ethernet modes 8-16, `pon_pll_cfg()` clears bits 4-5. The U-Boot baseline
already reads zero before network initialization and remains zero afterward.

The field is mode selection, not enough by itself to identify a CCF parent.
Initial PON SerDes setup may clear these bits through the TOPCRM syscon while
leaving every other bit untouched.

### Offset `0x44`: Uni SerDes Gate Control

| Bit | CCF clock | Parent | Selected path |
| --- | --- | --- | --- |
| 0 | `pon_serdes_pclk` | `sys_pclk`, 125 MHz | Required by PON SerDes and XPCS1 |
| 8 | `uni_serdes_pclk` | `sys_pclk`, 125 MHz | Not used: XMAC1 is routed through PON SerDes |

This clock must not be requested merely because the vendor helper is named
`uni_serdes_init`; the XMAC1 branch explicitly redirects to PON SerDes.

Vendor DT directly identifies bit 0 as `pon_serdes_pclk` with `sys_pclk` as its
parent. Vendor runtime reports 125 MHz. There is no independent `xpcs_pclk` in
the vendor clock tree; XPCS1 shares the PON SerDes PCLK domain rather than
receiving a fabricated second TOPCRM gate.

### Offset `0x48`: PON Gate Control

| Bit | Recovered operation | Current CCF | Decision |
| --- | --- | --- | --- |
| 10 | NPPT helper sets the bit | `pon_woe1_wclk`, parented by PON/NPPT mux and registered with `CLK_IGNORE_UNUSED` | NPPT consumer clock; keep out of isolated SerDes bring-up and consume from the future Ethernet driver |

The word remained `0x00001fd7` across the U-Boot network transition, so U-Boot
had enabled all required gates before the pre-network snapshot.

### Offsets `0xc0` and `0xc4`: PON PLL Mode Words

Recovered CPU133 Ethernet-mode programming writes:

```text
TOPCRM + 0xc0 = 0x20106454
TOPCRM + 0xc4 = 0x04000000
```

The U-Boot snapshots read:

```text
TOPCRM + 0xc0 = 0x60106454
TOPCRM + 0xc4 = 0x04000000
```

The unexplained `0x40000000` difference at `0xc0` proves that the recovered
`pon_pll_cfg()` body is not the entire persistent PLL state. The clean driver
must preserve unknown bits and must not replace `0xc0` with the recovered
literal blindly.

Initial implementation policy:

- Map only `0xc0..0xc7` as the PON SerDes PHY's non-overlapping mode resource.
- Verify the firmware profile and use masked writes only after each changed bit
  has a demonstrated purpose.
- Preserve bit 30 at `0xc0` until its semantics are recovered.
- Do not publish a programmable PLL clock or claim a 25 MHz parent yet.

## Reset Matrix

The existing `zte,zx296718-reset` implementation is active-low. This matches
the vendor sequence: clear a bit to assert reset and set it to deassert.

| Controller | ID/bit | Selected-path behavior | Decision |
| --- | --- | --- | --- |
| `local_reset@10e10070` | 0 | Asserted and deasserted around every PON SerDes mode setup | Add a named PON SerDes reset ID and consume it from the PHY driver |
| `local_reset@10e10070` | 1 | Asserted and deasserted with bit 0; U-Boot transition confirms `0 -> 1` | Add a named PON SerDes APB/config reset ID and consume it from the PHY driver |
| `top_reset@10e10060` | 9 | Toggled only on CPU132 in recovered `zx_pon_clk_reset_init()` | Not required on ZX279133 CPU133 selected path; leave untouched |

Vendor CPU133 order is:

1. Program PLL mode fields.
2. Clear local reset bit 0.
3. Clear local reset bit 1.
4. Execute ten vendor delay units.
5. Set local reset bit 0.
6. Set local reset bit 1.
7. Execute ten vendor delay units.
8. Program the 49-word SerDes mode profile and poll PLL/CDR lock.

The raw `__const_udelay(0x418958)` unit has not been converted to a portable
time duration. Mainline must use a documented conservative delay derived from
hardware validation, not copy the architecture-specific literal.

The generic reset controller has no `reset-us`, so consumers must use explicit
bulk assert, delay, and bulk deassert operations rather than
`reset_control_reset()`.

## Corrected Selected-Mode Sequence

The selected XMAC work mode 4 is 2.5GBASE-X. It calls
`uni_serdes_init(xmac=1, mode=5)`. The XMAC1 PON route then applies
`uni_eth_mode_change(5)`, which returns PON SerDes mode 9.

Mode 9 and the initial vendor P2P mode 15 both dispatch to the same recovered
2.5GBASE-X SerDes profile. Mode 10 is a different 2.5GBASE-R profile and is not
the selected initial path.

The clean implementation can initialize the final Ethernet mode 9 directly. It
does not need to reproduce the vendor's earlier failed PON/P2P mode-15 attempt
before XMAC setup.

For a 1 Gbit/s link, the recovered dynamic path uses the PHY host mode that
ultimately maps to PON SerDes mode 8. Supporting that transition belongs in the
SerDes/PHY phases after fixed 2.5G lock is proven.

## Non-Clock One-Time Configuration

### IDM CCI Setup

Recovered vendor Linux writes the full literal `0x00200020` to sysctrl offsets
`0x78` and `0x7c`. The U-Boot snapshots contain only `0x00000020` at both
locations.

This difference must be resolved before IDM DMA is enabled. The writes are not
CCF operations: they configure CCI/coherency or access behavior. They belong to
the future NPPT/IDM probe sequence through a syscon/regmap interface.

### PON SerDes to XMAC1 Route

Enabling the selected route performs three operations:

1. Set NPPT `0x19c` bit 0 through `greg_sdet_share_clk_cfg(1)`.
2. Clear PON `0x80` as the XMAC1 route state.
3. Clear NPPT `0x2438` bit 2.

These are cross-block routing controls, not CCF clocks. They belong to the
future NPPT Ethernet integration before the PON SerDes PHY is powered on.

## Minimal Implementation Model

The first clean clock/reset increment is deliberately small:

1. Add named dt-binding reset IDs for local reset bits 0 and 1.
2. Model TOPCRM `0x44` bit 0 as `pon_serdes_pclk`, parented by `sys_pclk`; both
   the PON SerDes PHY and XPCS1 consume this shared hardware clock.
3. Keep the existing PON/NPPT working-clock mux read-only.
4. Keep TOPCRM `0xc0/c4` outside the CCF resource and reserve it as a second,
   non-overlapping resource for the future PON SerDes PHY.
5. Do not add guessed IDM, TM, MAC, or independent XPCS gates until each
   gate bit and parent is independently established.
6. Do not use `CLK_IGNORE_UNUSED` for the represented network gates. Until real
   consumers land, CCF must disable PON SerDes, Uni SerDes, and WOE1 clocks.

This transitional dependency is acceptable for identifying the SerDes reset
and profile sequence. It is not acceptable as the final upstream driver state.

## Open Evidence Gaps

- Field semantics for PON PLL `0xc0` bit 30.
- Portable minimum assert/deassert and PLL/CDR poll delays.
- Whether TOPCRM `0x0c` bits 8-9 should ultimately be custom CCF controls or
  remain mode fields owned by the SerDes PHY.
- Exact IDM CCI meaning of sysctrl bit 21 versus bit 5.

## Phase 1.1 Result

Phase 1.1 is complete. Every selected-path clock/reset operation now has one
owner or is explicitly deferred as an evidence gap. The next implementation
item is limited to reset IDs and the existing clock/reset ownership boundaries;
it must not add guessed clock gates.

## Phase 1.2 Validation

Phase 1.2 added the two local-reset IDs and the evidence-backed
`pon_serdes_pclk` clock at TOPCRM `0x44` bit 0. The vendor DT identifies its
parent as `sys_pclk`; both vendor and mainline report 125 MHz.

The final test FIT was built through Docker and booted by TFTP. After
`clk_disable_unused()`:

```text
TOPCRM 0x44: 0x00000311 -> 0x00000210
TOPCRM 0x48: 0x00001fd7 -> 0x00001bd7
```

The first transition clears PON SerDes bit 0 and Uni SerDes bit 8. The second
clears WOE1 bit 10. Debugfs reports all three clocks disabled at their correct
rates, with no panic or BUG. Future PHY/PCS/Ethernet consumers must enable the
same gates through CCF rather than direct TOPCRM writes.
