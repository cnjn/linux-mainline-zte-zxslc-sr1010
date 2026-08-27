# SR1010 NPPT acceptance

This directory preserves the fixed-topology UDP NAT test used to accept the
ZX279133 NPPT backend. Throughput acceptance deliberately uses endpoint NIC
counters; `tc -s` can additionally verify the driver's per-flow hardware
packet, byte, and last-used reporting without any debugfs interface.

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
./port/mainline/nat-acceptance/prepare-pppd-root.sh
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

## IPv6 routed flowtable

The IPv6 test is pure routing: it does not use NAT66. Assign
`fd00:1::1/64` to the router WAN, `fd00:5::1/64` to `lan1`, and enable IPv6
forwarding. The WAN peer uses `fd00:1::100/64` with a route for `fd00:5::/64`
through the router WAN link-local address; the LAN peer uses
`fd00:5::100/64` with a route for the WAN peer through `fd00:5::1`.

```sh
ip -6 addr add fd00:1::1/64 dev eth0
ip -6 addr add fd00:5::1/64 dev lan1
sysctl -w net.ipv6.conf.all.forwarding=1
nft -f /tmp/nft-ipv6-flowtable.nft
```

Start an established TCP or UDP exchange between the two peers and require
both hardware directions to remain present while traffic is active:

```sh
grep 'HW_OFFLOAD' /proc/net/nf_conntrack
```

The IPv6 rule is deliberately separate from the IPv4/NAT ruleset. It admits
exact TCP and UDP routes only; IPv6 address or transport-header translation is
outside this acceptance.

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

Build `udp-echo.c` on macOS and use it with `UdpPaced.cs` when the test needs a
real reply rather than an `[UNREPLIED]` UDP connection:

```sh
clang -O2 -Wall -Wextra -Werror udp-echo.c -o /tmp/udp-echo
/tmp/udp-echo PORT
```

`flow-stats-reuse.sh` runs repeated add, zero-stats, delete, and counter-ID
reuse cycles. `flow-stats-capacity.sh` has `add`, `check`, and `del` commands for
a concurrent rule set. `flow-zcam-collision.sh` uses 32 port pairs whose
forward and reverse keys each share one primary ZCAM location, forcing the
allocator through alternate cells and blocks. Copy the scripts and
`tc-udp-flow.sh` to the board and set `TC`/`HELPER` when they are not under
`/tmp`.

The complete capacity and collision checks are:

```sh
TC=/tmp/tc HELPER=/tmp/tc-udp-flow.sh \
	/tmp/flow-stats-capacity.sh add 2048 1000 6000
TC=/tmp/tc /tmp/flow-stats-capacity.sh check 2048 1000 6000
TC=/tmp/tc HELPER=/tmp/tc-udp-flow.sh \
	/tmp/flow-stats-capacity.sh del 2048 1000 6000

TC=/tmp/tc HELPER=/tmp/tc-udp-flow.sh \
	/tmp/flow-zcam-collision.sh add 4000
TC=/tmp/tc /tmp/flow-zcam-collision.sh check 4000
TC=/tmp/tc HELPER=/tmp/tc-udp-flow.sh \
	/tmp/flow-zcam-collision.sh del 4000
```

The 2,048-connection test consumes all 4,096 IKEY/age entries. The first 1,024
directions have exact packet/byte counters; later directions intentionally
report zero packet/byte deltas and use hardware age for `lastused`.

The accepted statistics lifecycle used 100 reuse cycles and 256 concurrent NAT
connections. Eight distributed connections each counted 825 packets and
1,252,350 hardware bytes in both directions; untouched sample connections
remained at zero, and deleting all 256 connections left no rule behind.

## Bridge and access VLAN

For a bridge test, remove the LAN address from `lan1`, enslave it to `br0`, and
assign `192.168.5.1/24` to `br0`. Keep `lan1` in the nftables flowtable device
set because it remains the physical ingress. The same UDP connection must still
reach `[HW_OFFLOAD]`.

The RTL8372N driver supports an untagged access VLAN over its private CPU-link
transport. Set the bridge default PVID to 0, add the customer VID as
PVID/untagged on `lan1` and the bridge self port, then enable VLAN filtering.
The LAN peer stays untagged. Tagged trunk VLANs are not part of this acceptance.

For software fallback, send ICMP through the routed SNAT path. Ping must work,
and the matching conntrack entry must not contain `[HW_OFFLOAD]` because the
flow-add rule admits only established TCP and UDP.

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

## PPPoE hardware offload

Use an Alpine arm64 HVF VM with a virtio-net WAN attached to the physical Mac
interface through QEMU `vmnet-bridged`:

```text
-device virtio-net-pci,netdev=wan \
-netdev vmnet-bridged,id=wan,ifname=en8
```

Remove every IPv4 address from the Mac interface while it is acting as this
layer-2 bridge. The VM Ethernet interface needs no IP address; the PPP endpoint
owns `192.168.1.100` and assigns `192.168.1.1` to the router. Replace `eth1`
below if the virtio-net WAN has another name:

```sh
apk add ppp rp-pppoe iperf3 tcpdump
modprobe pppoe
ip link set eth1 up
pppoe-server -k -g /usr/lib/pppd/2.5.3/pppoe.so -q /usr/sbin/pppd \
	-I eth1 -L 192.168.1.100 -R 192.168.1.1 -N 1 \
	-O pppoe-server-options -C zx279133-test
iperf3 -s -D
```

Do not use QEMU `usb-host` passthrough of a Realtek RTL8156 for performance
acceptance. Its virtual xHCI/r8152 receive path reported `rx_missed` for every
missing packet and reduced single-flow TCP to a few hundred Mbit/s. With
vmnet-bridged, identify the virtio WAN input and output IRQs in
`/proc/interrupts` and pin them to separate vCPUs before measuring throughput.

On the router, remove the diagnostic IPv4 address from the physical WAN,
bring up the LAN DSA port, establish PPPoE, and load the physical-device
flowtable. The routed output remains `ppp0`; the flowtable ingress devices are
`eth0` and `lan1` so the driver receives the underlying hardware paths.

```sh
ip addr flush dev eth0
modprobe tag_zx279133_rtl8372n
modprobe zx279133-rtl8372n
ifconfig lan1 192.168.5.1 netmask 255.255.255.0 up
pppd plugin pppoe.so eth0 noauth noipdefault mtu 1492 mru 1492 nodetach &
nft -f /etc/nft-pppoe-flowtable.nft
```

Require `ppp0` to show local `192.168.1.1`, peer `192.168.1.100`, and session
ID 1. Capture the WAN peer with:

```sh
tcpdump -i eth1 -ne
```

Both directions must remain PPPoE session frames with SID 1; no forwarded
`0x0800` frame is allowed on the physical WAN. Test bulk TCP in both directions
and UDP with `iperf3`. Both directions use hardware flow entries and the
aggregate conntrack must report `[HW_OFFLOAD]`. PPPoE pop uses compact SDT43;
PPPoE push uses the complete SDT14 response after applying the response-word
conversion used by the factory driver's full-table fallback. A WAN capture of
an offloaded upload must show the translated source `192.168.1.1` inside a
PPPoE session frame.

The accepted vmnet-bridged fixture sustained 2.31 Gbit/s single-flow TCP from
the PPPoE peer to Windows with zero retransmissions. The matching conntrack
entry remained `[HW_OFFLOAD]`. In the opposite direction, the VM's single
virtio RX queue and software PPPoE receive path levelled off near 0.94 Gbit/s;
even four TCP flows did not raise the aggregate. Treat that direction as
functional and hardware-hit acceptance, not as a line-rate limit of the router.

For lifecycle acceptance, flush nftables while a hardware connection is
active. WANID 0 must return from push mode to its saved session/mode and WANID
16 from pop mode to its saved state immediately; a newly created flowtable
must offload a new connection without rebooting.

## Cleanup

```sh
TC=/tmp/tc /tmp/tc-udp-flow.sh del 5202 2
```

Re-run the UDP/5000 echo check, confirm only the canonical rule remains on
each ingress device, and check dmesg for `BUG`, `Oops`, `WARNING`, WANID, or
SMMU timeout messages.
