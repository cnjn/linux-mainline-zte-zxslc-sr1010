# ZX279133 First Mainline Network Path

## Decision

The first mainline bring-up path is the SR1010 board-labeled WAN copper port,
represented by vendor netdev `eth0` and logical UNI port 5.

The selected hardware path is:

```text
Linux netdev
  <-> IDM CPU RX/TX queues
  <-> NPPT forwarding, descriptor outport 6
  <-> XMAC1
  <-> XPCS1 at 0x1b000000
  <-> ZX279133 PON SerDes at 0x16000000
  <-> ZX279051 external PHY
  <-> board WAN connector
```

This path is selected because it avoids the RTL8372N switch and has both vendor
runtime evidence and prior board-level mainline validation. ZX279051 is on the
critical path and cannot be deferred beyond link-layer bring-up.

## Confidence

- **Verified topology:** XMAC1, XPCS1, PON SerDes, ZX279051, UNI5, NPPT output
  port 6, and the vendor `eth0` WAN role agree across recovered control flow,
  vendor runtime logs, module disassembly, and prior board validation notes.
- **Verified management path:** ZX279051 is accessed through MDIO controller 1
  at PHY address 1, using MMD 7 for the vendor identification register.
- **Verified initial modes:** XMAC1 boots in work mode 4, 2.5GBASE-X. A captured
  1 Gbit/s link transition reconfigures the PHY, PON SerDes, XPCS, and XMAC1.
- **Unresolved root reference:** the selected SerDes uses the TOPCRM PON PLL
  Ethernet profile, but the PLL's root reference source is not conclusively
  established as a mainline clock input.

## Component Selection

| Component | Selection | Address or identifier | Evidence |
| --- | --- | --- | --- |
| Physical connector | Board-labeled WAN copper port | Vendor `eth0` | Factory interface state, `sw_set_uni_as_wan uni=5`, README boot instructions, prior board tests |
| Logical NPPT port | UNI5 | Vendor logical port 5 | Vendor boot initializes `eth0, uni5`; switch capability reports WAN port 5 |
| CPU TX destination | XMAC1 path | Descriptor outport 6 | `get_eth_wan_port() + 1` produces corrected `lan_up_port=6`; prior descriptor tests confirm only outport 6 reaches XMAC1 |
| XMAC | XMAC1 | XMAC index 1 | Product mode table plus runtime work-mode and link-change logs |
| PCS | XPCS1 | `0x1b000000` | XPCS base `0x1a000000` plus the recovered `xmac << 24` instance stride |
| SoC SerDes | PON SerDes routed to XMAC1 | `0x16000000` | `ponserdes_to_xmac1_en_set()`, `uni_serdes_init(xmac=1)`, and runtime PON SerDes lock sequence |
| SerDes PLL | PON SerDes PLL | `0x16010000`, plus TOPCRM PLL registers | Vendor DT and recovered SerDes/PLL setup |
| External PHY | ZTE ZX279051 | Detected ID value `0x84b9` | `zx279051.ko` init disassembly and runtime initialization logs |
| MDIO controller | MDIO1 | `0x14f02000`, vendor bus ID 1 | Vendor DT and boot log |
| MDIO address | PHY address 1 | MDIO1, PHY 1 | `zx279051.ko` calls the MDIO1 callback with `(phy=1, devad=7, reg=2)` and compares the result with `0x84b9` |
| Initial host interface | 2.5GBASE-X | XMAC work mode 4 | Recovered work-mode table and runtime `xmac 1 work mode 4` |
| 1 Gbit/s host interface | Dynamic 1G/S(P)GMII-compatible path | Recovered PHY SerDes mode 6 and XMAC1 transition | Runtime lines reporting PHY mode 6, PON SerDes lock, and XMAC1 speed update |

## Vendor Product Selection Evidence

The captured board reports product ID 23 (`0x17`) and name SR1010. Disassembly
of the vendor kernel's `xmac_phy_type()` returns raw type 5 for product ID 23.

Recovered `nppt_smac_init()` maps PHY type 5 to:

| XMAC | Work mode | Meaning |
| --- | --- | --- |
| XMAC0 | 5 | 10G USXGMII auto |
| XMAC1 | 4 | 2.5GBASE-X |

The factory runtime independently logs `xmac 0 work mode 5` and
`xmac 1 work mode 4`.

When `Is_279051_phy == 1`, the same type-5 branch assigns the vendor ZX279051
PHY handling to XMAC1. `phy_zxic051_port_exist()` accepts only logical PHY port
5, and the runtime later reports its state change through XMAC ID 1.

## PON SerDes Routing Evidence

The SR1010 factory configuration reports PON work mode `0x10`, P2P. Probe then
sets `lan_up`, obtains WAN logical port 5, corrects the internal destination to
port 6, and enables the PON-SerDes-to-XMAC1 route.

The route operation is explicit:

- PON offset `0x80` is cleared when enabled.
- NPPT offset `0x2438` bit 2 is cleared when enabled.
- `g_ponserdes_to_xmac1` is set.

For XMAC1, `uni_serdes_init()` does not program the Uni SerDes block. When this
route is enabled it maps the requested Ethernet mode to a PON SerDes mode and
calls `zx_pon_clk_reset_init()`.

The initial XMAC1 work mode 4 maps to the PON SerDes Ethernet 2.5GBASE-X
profile. Runtime shows this exact sequence before logging XMAC1 mode 4. During a
later 1 Gbit/s link transition, runtime shows:

1. ZX279051 private 1G profile setup.
2. PHY stable and PLL lock.
3. XMAC1 selected as the affected XMAC.
4. PON SerDes Ethernet 1G setup and successful PLL/CDR lock.
5. XMAC1/XPCS speed reconfiguration.
6. `eth0` link ready at 1000 Mbit/s full duplex.

## ZX279051 Management Evidence

`zx279051.ko` initializes before `plat_132.ko`. Its module entry performs reset
sequencing for WAN and LAN external PHYs, then calls the MDIO1 extended-read
callback with:

```text
MDIO controller    = MDIO1 (`zx_mdio_1_read_extended`)
PHY address        = 1
MMD / register     = 7 / 2
expected value     = 0x84b9
```

On a match it initializes logical port 5 and sets exported
`Is_279051_phy = 1`. The driver uses private extended MDIO accesses and dynamic
SerDes mode handling, so a generic PHY driver alone is not sufficient for the
known-good 1G/2.5G behavior.

The vendor runtime has no registered PHYLIB MDIO device. That is a vendor
architecture detail, not a mainline model: the mainline implementation should
describe PHY address 1 under MDIO1 and implement ZX279051 as a separate PHYLIB
driver.

Mainline hardware validation confirms that standard C22/C45 discovery cannot
identify the PHY, so the board uses `ethernet-phy-id0000.84b9` to create the
PHYLIB device. The driver verifies private MMD7:2 at probe, then reads Clause 22
GE status register 26. `zx279051.ko` establishes bit 6 as link and bits 9:7 as
the resolved link-mode code. On the validated 2.5G link the register reads
`0x1347`, yielding link up, code 6, and full duplex; this matches U-Boot.

## Netdev and Connector Correlation

The factory boot creates these logical interfaces:

| Interface | Logical port | Role in the captured system |
| --- | --- | --- |
| `eth0` | UNI5 | WAN physical port with ZX279051; carrier up |
| `eth1` | UNI58 | RTL8372N LAN port |
| `eth2` | UNI59 | RTL8372N LAN port |
| `eth3` | UNI60 | RTL8372N LAN port |
| `eth4` | UNI61 | RTL8372N LAN port |

The switch module reports WAN port 5 and explicitly logs
`sw_set_uni_as_wan uni=5`. Runtime then reports `mac 5 phy status changed` and
`eth0: link becomes ready` in the same link transition.

The project README's boot procedure also instructs connecting the host to the
board WAN port. Prior board validation notes identify this connector as the
tested mainline path and record successful 1G and 2.5G link, ping, RX/TX, and
link-LED tests.

Those prior notes are validation history, not source present in the current
branch. No prior Ethernet or ZX279051 driver source is currently reachable from
the repository's Git refs.

## Why XMAC0 Is Not First

XMAC0 runs 10G USXGMII auto mode through the Uni SerDes. The SR1010 factory
system also loads the RTL8372N switch and RTL8226B companion PHY before NPPT
bring-up, and exposes LAN interfaces through logical UNI ports 58-61.

Bringing up XMAC0 first would therefore combine SoC MAC/PCS/SerDes work with an
external switch driver, switch CPU-port configuration, and DSA topology. It has
a larger failure surface and does not satisfy the goal of validating one direct
physical netdev before LAN switching.

XMAC0 remains the first DSA uplink candidate after the XMAC1 WAN path is stable.

## Reference Clock Status

The selected path uses the PON SerDes PLL and the TOPCRM Ethernet-mode profile:

- `pon_pll_cfg()` handles mode range 8-16 as Ethernet.
- XMAC1 work mode 4 calls Uni mode 5, which maps to PON SerDes mode 9.
- The profile programs TOPCRM offsets `0x10`, `0xc0`, `0xc4`, and `0x0c` before
  the PON SerDes reset and lock sequence.

There is a recovered helper for configuring the PON PLL from a 25 MHz external
reference, but it has no direct caller in `plat_132.ko`. It therefore does not
prove that the selected runtime path uses the 25 MHz oscillator.

**Implementation constraint:** Phase 1 must preserve the captured bootloader or
firmware PLL state until the TOPCRM field and parent source are validated. The
initial binding must not claim a fixed 25 MHz SerDes reference solely from the
unused helper name.

## Mainline Bring-Up Contract

The first implementation should support only this path:

1. Enable MDIO1 and instantiate ZX279051 at address 1.
2. Expose PON SerDes and its PLL/configuration dependencies through a generic
   PHY provider.
3. Use XPCS1 and XMAC1 only; leave XPCS0/XMAC0 and RTL8372N disabled.
4. Support the host interface transitions required for 1G and 2.5G links.
5. Direct NPPT TX descriptors to outport 6.
6. Bring up one normal Linux netdev with no PON pseudo-netdevs and no offloads.

The implementation must not assume the vendor `eth0` name as an ABI. The board
WAN connector, PHY, MAC, PCS, and SerDes relationships belong in DT and phylink.

## Remaining Validation Before Register-Level Coding

- Read and record the selected TOPCRM PON PLL parent/reference field on a cold
  vendor boot and after a 1G/2.5G link transition.
- Confirm that MDIO controller 1 maps to mainline `mdio1` numbering regardless
  of probe order.
- Preserve a cold-boot vendor register snapshot for PON SerDes, XPCS1, XMAC1,
  MDIO1, and the ZX279051 private SerDes state.
- Record the Ethernet peer configuration used for both 1G and 2.5G captures.

These are Phase 0.3 capture tasks. They do not change the selected path.

## Phase 0.2 Result

Phase 0.2 is complete. The first path is XMAC1/XPCS1/PON SerDes/ZX279051 on
UNI5, managed through MDIO1 address 1 and exposed at the board WAN connector.
ZX279051 is a required first-path driver. RTL8372N, XMAC0, and the LAN ports are
deferred.
