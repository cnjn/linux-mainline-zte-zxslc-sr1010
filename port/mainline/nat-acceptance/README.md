# SR1010 UDP NAT acceptance

This directory preserves the fixed-topology UDP NAT test used to accept the
ZX279133 NPPT backend. It deliberately uses standard TC flower rules and host
NIC counters; it does not depend on driver debugfs or hardware statistics.

## Topology

| Role | Interface | Address | MAC |
| --- | --- | --- | --- |
| WAN peer | Mac `en8` | `192.168.1.100/24` | `00:e0:41:68:0d:86` |
| Router WAN | `eth0` | `192.168.1.1/24` | assigned at boot |
| Router LAN | `lan1` | `192.168.5.1/24` | DSA port |
| LAN peer | Windows | `192.168.5.100/24` | `f8:89:3c:26:fe:02` |

Stage a newly built FIT once with:

```text
tftpboot sr1010-zxdbg.itb; bootm
```

For every subsequent reset acceptance, restart from Linux and reuse the FIT
already retained at `0x88000000`:

```text
Linux:  reboot -f
U-Boot: bootm
```

The acceptance boot must not run `tftpboot`: its U-Boot network path
initializes the WAN PHY and SerDes. U-Boot `reset` does not retain a valid FIT
on this board, so it cannot replace the Linux `reboot -f` step.

Build the pinned static nftables binary before rebuilding the diagnostic FIT:

```sh
./port/mainline/nat-acceptance/build-nft-static.sh
./port/mainline/build-zxdbg.sh
```

All board changes below are runtime-only.

## Board preparation

Load the DSA modules, configure the LAN address, and copy `tc` plus
`tc-udp-flow.sh` into `/tmp` through TFTP. Then install the canonical flow:

```sh
modprobe tag_zx279133_rtl8372n
modprobe zx279133-rtl8372n
ifconfig lan1 192.168.5.1 netmask 255.255.255.0 up
TC=/tmp/tc /tmp/tc-udp-flow.sh add 5000 1
```

Use UDP port 5000 for the `10/10` echo check and port 5202 for performance:

```sh
TC=/tmp/tc /tmp/tc-udp-flow.sh add 5202 2
```

Both devices must report `in_hw` and `in_hw_count 1` for the installed rule.

## Automatic nftables flowtable

Do not install the manual TC rules for this test. The diagnostic image embeds
a static `nft` binary, enables IPv4 forwarding, and provides conntrack procfs.
Load the standard ruleset with:

```sh
nft -f /tmp/nft-flowtable.nft
```

Start a normal UDP or TCP connection from Windows to the WAN peer. After
bidirectional traffic has established conntrack, verify hardware offload with:

```sh
grep HW_OFFLOAD /proc/net/nf_conntrack
```

The ruleset contains ordinary SNAT and `flow add`; it does not encode a
five-tuple or any pedit action. To test deletion while retaining conntrack:

```sh
nft flush ruleset
grep 'ASSURED' /proc/net/nf_conntrack
```

The `flow add` rule is gated by `ct state established`. One-way UDP remains in
software until the first real reply reaches conntrack, avoiding an
`[UNREPLIED]` hardware flow that expires on the short UDP timeout.

## LAN-to-WAN sender

Copy `UdpPaced.cs` to Windows and load it from PowerShell. The following sends
one five-tuple for 30 seconds at a 2.46 Gbit/s NIC-counter target:

```powershell
Add-Type -Path C:\Users\cnjn\AppData\Local\Temp\UdpPaced.cs
[UdpPaced]::Run(30000, 1472, 5202, 2460000000, 4)
```

Read Windows transmit counters before and after the run:

```powershell
Get-NetAdapter -InterfaceIndex 19 | Get-NetAdapterStatistics
```

Read Mac receive counters from the `<Link#...>` row:

```sh
netstat -ibnI en8
```

## WAN-to-LAN sender

Build and run the macOS sender. Its `SO_REUSEPORT` sockets all bind the same
source address and port and send to the same destination, so the wire still
contains one UDP five-tuple.

```sh
clang -O3 -pthread udp-pacer.c -o /tmp/udp-pacer
/tmp/udp-pacer 30 2460000000
```

Optional arguments select the local port, sender delay in milliseconds,
translated router port, and UDP payload length:

```sh
/tmp/udp-pacer SECONDS LINK_BPS LOCAL_PORT DELAY_MS REMOTE_PORT PAYLOAD_BYTES
```

A zero link rate runs only the four receive-drain sockets. Each socket replies
to its first packet so conntrack becomes established before hardware offload.

Compare Mac transmit bytes with Windows `ReceivedBytes` and compare the
sender's packet count with Windows `ReceivedUnicastPackets`.

For a minimum-sized Ethernet frame, use an 18-byte UDP payload. Both harnesses
report transmit and receive PPS directly; endpoint NIC counters remain the
acceptance source because a userspace receiver can drop after the NIC accepted
the frame.

## Simultaneous directions and packet rate

Start the macOS command first. Its receive threads immediately drain the
LAN-to-WAN stream and send the first reply; the delay then gives conntrack and
the hardware flowtable time to install both directions before macOS starts
sending:

```sh
/tmp/udp-pacer 10 1800000000 PORT 1500 PORT
```

Run the LAN endpoint for 11.5 seconds so it covers the delayed ten-second WAN
sender interval:

```powershell
[UdpPaced]::Run(11500, 1472, PORT, 1800000000, 4)
```

For the minimum-frame packet-rate test, leave macOS in receive-only mode and
send 18-byte payloads from Windows:

```sh
/tmp/udp-pacer 12 0 PORT 0 PORT 18
```

```powershell
[UdpPaced]::Run(10000, 18, PORT, 2460000000, 8)
```

Read aggregate CPU counters from the router's first `/proc/stat` line before
and after the traffic interval. Treat the resulting packet rate as a tested
floor unless the endpoint NIC transmit counter proves that the sender reached
minimum-frame wire rate.

## Multi-flow lifecycle

Use a separate port per connection and keep each sender below line rate. During
the run, filter `/proc/net/nf_conntrack` by those ports and require every entry
to show `[HW_OFFLOAD]`. After traffic stops, the hardware marker must disappear
at `nf_flowtable_udp_timeout` while conntrack remains alive for its normal UDP
timeout.

Flushing the complete nftables ruleset must remove every hardware marker
without deleting conntrack. Recreating the flowtable does not retroactively
offload connections owned by the destroyed table, so use a new connection for
the post-flush allocation check; it must reach `[HW_OFFLOAD]` without rebooting
or reloading the network driver.

## Single-flow TCP NAT

Load `TcpStream.cs` on Windows. Windows always opens one connection through
the router, while the direction selects which endpoint writes payload data:

```powershell
Add-Type -Path C:\Users\cnjn\AppData\Local\Temp\TcpStream.cs
[TcpStream]::Send(15000, 5205)
[TcpStream]::Receive(15000, 5206)
```

For `Send`, run a TCP sink on Mac port 5205. For `Receive`, run a TCP source on
Mac port 5206. During each transfer, the matching conntrack entry must contain
`[HW_OFFLOAD]`; after the connection closes, the entry must disappear.

## Cleanup

```sh
TC=/tmp/tc /tmp/tc-udp-flow.sh del 5202 2
```

Re-run the UDP/5000 echo check, confirm only the canonical rule remains on
each ingress device, and check dmesg for `BUG`, `Oops`, `WARNING`, WANID, or
SMMU timeout messages.
