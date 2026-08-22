# SR1010 Vendor U-Boot Network Baseline

## Scope

This is the reproducible Phase 0.3 hardware baseline for the selected SR1010
WAN path. It compares the patched vendor U-Boot state immediately after `reset`
with the state after U-Boot initializes `eth0` for a network command.

It is not a Linux driver dump and it is not a power-on silicon-reset image.
U-Boot has already performed its early SoC, clock, MDIO, and board setup before
the first snapshot.

No stale Linux FIT was booted during this capture.

## Environment

- Date: 2026-07-29.
- Serial: `/dev/cu.usbmodem5B140649641`, 115200 baud through `tio`.
- tmux session: `sr-mainline`.
- U-Boot local address: `192.168.1.1`.
- Ethernet peer: `192.168.1.100`, connected to the board WAN port.
- Product: SR1010, product ID 23.
- Selected vendor device: `eth0`, logical UNI5, internal output port 6.

## Reproduction

1. At the U-Boot prompt, run `reset` and wait for the next `=>` prompt.
2. Read the pre-network registers listed below before running any network
   command.
3. Run `tftpboot sr1010-zxdbg.itb` to trigger vendor Ethernet initialization.
4. Abort the transfer after link initialization if no TFTP service is running.
5. Read the same registers again.
6. Run `ping 192.168.1.100` to verify bidirectional packets.

The captured TFTP request timed out because no TFTP daemon was active. The
transfer was aborted at U-Boot and `bootm` was not run. The subsequent U-Boot
ping succeeded with `host 192.168.1.100 is alive`.

## Observed Link

U-Boot reported:

```text
PHY name: phy_zxic_051
logical port: 5
link: 1
speed: 3 (logged as 2.5G)
duplex: 1 (full duplex)
internal MAC/output port: 6
device: eth0
```

This independently reconfirms the Phase 0.2 selection of UNI5, ZX279051,
XMAC1, and NPPT output port 6.

U-Boot did not expose packet counters in this command path. Traffic validation
for this baseline is one successful ICMP request/reply, not a throughput or
loss test.

## Targeted Register Comparison

| Register | Pre-network | Post-initialization | Interpretation |
| --- | --- | --- | --- |
| TOPCRM `0x10e1000c` | `0x07711177` | `0x06711277` | Network clock/mux field changed; exact field ownership remains under Phase 1.1 audit |
| TOPCRM `0x10e10010` | `0x00000000` | `0x00000000` | No change in this capture |
| TOPCRM `0x10e10044` | `0x00000311` | `0x00000311` | Uni SerDes PCLK register unchanged |
| TOPCRM `0x10e10048` | `0x00001fd7` | `0x00001fd7` | PON/NPPT gate register unchanged |
| top reset `0x10e10060` | `0x00002fff` | `0x00002fff` | No change in this capture |
| local reset `0x10e10070` | `0x00001311` | `0x00001313` | Bit 1 set during selected-path initialization |
| PON PLL `0x10e100c0` | `0x60106454` | `0x60106454` | Programmed before the pre-network snapshot |
| PON PLL `0x10e100c4` | `0x04000000` | `0x04000000` | Programmed before the pre-network snapshot |
| sysctrl `0x10e00078` | `0x00000020` | `0x00000020` | CCI-related state already established |
| sysctrl `0x10e0007c` | `0x00000020` | `0x00000020` | CCI-related state already established |
| PON route `0x17000080` | `0x00000001` | `0x00000000` | PON SerDes route to XMAC1 enabled |
| NPPT route `0x19002438` | `0x00000003` | `0x00000003` | Already in the required state before network command |
| NPPT reset `0x192c0004` | `0xffffffff` | `0xffffffff` | Reset pulse completed before each visible snapshot |
| XMAC1 speed control `0x19180000` | `0x00000000` | `0xc0010000` | XMAC1 host speed/mode programmed |
| XMAC1 duplex control `0x19180500` | `0x00000100` | `0x00000100` | Full-duplex-related state unchanged |
| XPCS1 PMA ID words | `0x0000:0x0000` | `0x0000:0x0000` | No change; zero PMA ID requires later interpretation |
| XPCS1 PCS ID words | `0x7996:0xced0` | `0x7996:0xced0` | DesignWare PCS identity visible before network init |
| XPCS1 AN ID words | `0x0000:0x0000` | `0x0000:0x0000` | No change |
| XPCS1 digital status `dev3:0x8010` | `0x0010` | `0x0010` | No change in the sampled word |

### XPCS Address Encoding

The sampled XPCS registers use the recovered and previously audited direct
mapping:

```text
address = XPCS1_BASE + ((((devad << 16) | reg) * 4))
XPCS1_BASE = 0x1b000000
```

The unchanged XPCS words do not prove that no other XPCS register changed.
They only bound the sampled identity and digital-status locations.

## PON SerDes Profile Difference

The selected path rewrites 31 of the 49 sampled words in the PON SerDes
`0x16000000..0x160000c0` profile:

| Address | Pre-network | Post-initialization |
| --- | --- | --- |
| `0x16000000` | `0xc0000004` | `0xe0000004` |
| `0x16000004` | `0x50a840a7` | `0x4fa8c0a2` |
| `0x16000008` | `0x012e064f` | `0x013e8604` |
| `0x1600000c` | `0x1fd0c093` | `0x1f51c8f3` |
| `0x16000014` | `0x00000000` | `0x00194000` |
| `0x16000018` | `0x00f00000` | `0x0b080000` |
| `0x1600001c` | `0x00230120` | `0x00238220` |
| `0x16000020` | `0x00000000` | `0x80000100` |
| `0x16000024` | `0x00020000` | `0x00050000` |
| `0x16000028` | `0x0c633930` | `0x0c633830` |
| `0x1600002c` | `0x00000007` | `0x00000000` |
| `0x16000034` | `0x00000000` | `0x00002000` |
| `0x1600003c` | `0x00000000` | `0x00ff0000` |
| `0x16000040` | `0x05a85400` | `0x05a8d100` |
| `0x16000048` | `0x8f000b6a` | `0x04005b6a` |
| `0x1600004c` | `0x60002200` | `0x60002220` |
| `0x16000050` | `0x00000003` | `0x34000003` |
| `0x16000054` | `0x00000001` | `0x00000409` |
| `0x16000060` | `0x200554a8` | `0x200574a8` |
| `0x16000064` | `0x3df48091` | `0x00649052` |
| `0x1600006c` | `0xc0003700` | `0x40007700` |
| `0x16000074` | `0x10108000` | `0x01108002` |
| `0x16000090` | `0x00004000` | `0x0000401c` |
| `0x16000094` | `0x0000e000` | `0x802dc000` |
| `0x1600009c` | `0x33333300` | `0x55555500` |
| `0x160000a0` | `0x33333333` | `0x55555555` |
| `0x160000a4` | `0x00333333` | `0x00555555` |
| `0x160000a8` | `0x20000818` | `0x30000818` |
| `0x160000ac` | `0x0000201c` | `0x40002000` |
| `0x160000b0` | `0x0000000c` | `0x20200000` |
| `0x160000b4` | `0x01000000` | `0x00808020` |

The post-initialization profile matches the observed 2.5G full-duplex link
state. These values are evidence for the initial SerDes PHY implementation, but
their individual field names remain unknown unless separately established.

## Clock Reference Finding

TOPCRM PON PLL words `0xc0` and `0xc4` are already
`0x60106454`/`0x04000000` after U-Boot reset and do not change when Ethernet is
initialized. The network command changes TOPCRM `0x0c` instead.

Therefore this capture proves that the selected path consumes an already
configured PON PLL profile; it does not prove the PLL's root reference source.
The mainline driver must not depend indefinitely on U-Boot state, but Phase 1
must first identify the `0x0c`, `0xc0`, and `0xc4` field ownership before
programming them.

## Build and Artifact Constraint

The existing `out/sr1010-zxdbg.itb` was built on 2026-07-23 and contains the
intentionally deleted legacy network drivers. It was loaded only far enough for
U-Boot to attempt TFTP and was never executed.

All future test images must be rebuilt from the current source tree through
`port/mainline/build-zxdbg.sh`, which re-executes inside Docker image
`sr1010-stage15-builder`.

## Phase 0.3 Result

Phase 0.3 is complete for the initial 2.5G path. The pre-network and
post-initialization states are reproducible after U-Boot `reset`; the selected
WAN link reaches 2.5G full duplex and exchanges ICMP with the configured peer.
Packet-counter and 1G transition captures remain useful later tests but do not
block the first clean clock/reset and SerDes implementation.
