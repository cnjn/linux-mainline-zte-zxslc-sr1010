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

Boot the FIT from the patched U-Boot shell with:

```text
tftpboot sr1010-zxdbg.itb; bootm
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

Compare Mac transmit bytes with Windows `ReceivedBytes` and compare the
sender's packet count with Windows `ReceivedUnicastPackets`.

## Cleanup

```sh
TC=/tmp/tc /tmp/tc-udp-flow.sh del 5202 2
```

Re-run the UDP/5000 echo check, confirm only the canonical rule remains on
each ingress device, and check dmesg for `BUG`, `Oops`, `WARNING`, WANID, or
SMMU timeout messages.
