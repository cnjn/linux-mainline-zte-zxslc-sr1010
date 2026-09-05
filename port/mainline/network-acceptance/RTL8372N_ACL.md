# RTL8372N tc-flower ACL offload

This gate covers the RTL8372N ingress ACL exposed through the standard DSA
`cls_flower` callbacks. It is separate from the NPPT routed-flow offload and
from the port-wide matchall ingress policer.

## Hardware contract

The register and table contract comes from `lotusmomo/airoha_sdk` commit
`32b5aa356c2406edef8806aaf75451ff0a5286f2`, primarily:

- `private/ether/en7512/rtl8372n/dal/rtl8373/dal_rtl8373_acl.c`
- `private/ether/en7512/rtl8372n/dal/rtl8373/dal_rtl8373_acl.h`
- `private/ether/en7512/rtl8372n/dal/rtl8373/rtl8373_reg_definition.h`

The implementation owns all 96 physical ACL slots. It initializes five
templates and all 16 parser field selectors, enables ACL lookup only on DSA
user ports, leaves unmatched traffic permitted, and keeps logical rules ordered
by tc priority. A logical flower rule may occupy multiple consecutive physical
slots; the first slot carries the action and the remaining slots extend the
match, as in the DAL programming model. Capacity accounting is therefore in
physical slots rather than flower-cookie count.

The SDK's template 2/3 field list and its full-address write loop disagree: its
selectors include IPv6 fixed-header and L4 words, so writing eight consecutive
address words does not match a complete address. Hardware HSB capture confirmed
the discrepancy. The driver uses the native 32-bit SIP/DIP fields for the low
32 bits and six `FORMAT_IPV6` selectors for the high 96 bits of each address.
This gives lossless 128-bit source and destination matching while retaining the
DAL table/action format.

The TCAM uses the DAL X/Y encoding, not a plain value/mask pair. Source-port
selection is also non-obvious: `activePmsk.mask` must cover all ten hardware
ports while `activePmsk.value` contains the single ingress port. On deletion,
the slot must be rewritten as the DAL's all-don't-care invalid rule and its
action control restored to `0xff`; writing a zero-filled entry prevents later
slot reuse from matching correctly.

## Linux interface

Supported on ingress chain 0 with `skip_sw`:

- template 0: EtherType and arbitrarily masked source/destination MAC;
- template 1: IPv4 source/destination address, IP protocol, TOS, and exact or
  masked TCP/UDP source/destination ports;
- templates 2/3: fully masked 128-bit IPv6 source/destination address, traffic
  class, next-header, and exact or masked TCP/UDP ports;
- template 4: 802.1Q C-tag, 802.1ad S-tag, stacked S+C tags, and TCP/UDP source
  or destination ranges through the 16-entry hardware port-range table;
- actions: pass, drop, mirror or redirect to another RTL8372N user port,
  trap to the CPU, `skbedit priority 0..7`, and per-rule shared-meter police;
  priority may be combined with a forwarding action or police;
- IPv4/IPv6 DSCP remark through standard extended `pedit`, with ECN retained;
  an optional IPv4-header `csum` action is accepted because the ASIC updates
  the checksum as part of the hardware remark;
- 802.1Q C-VLAN push, pop, and ID/PCP modification through the standard
  `vlan` action;
- delayed `tc -s` packet statistics and `lastused` through the 32 hardware ACL
  logging counters; `hw_stats disabled` leaves those counters free;
- add, priority ordering, replace by cookie, delete, compaction, and slot reuse.

The driver rejects unsupported keys or actions instead of silently falling
back when `skip_sw` is requested.

## Board acceptance, 2026-09-03 to 2026-09-04

Topology: Windows `192.168.5.100` on `lan1`, router `192.168.5.1`; `lan2` was
enabled as the mirror/redirect destination but had no attached peer.

Final combined FIT:

```text
c32be49f80a5e8b257bb6f5bf1bd251ec6dbdb2ce273a4aff0b75ec825a4839b  sr1010-zxdbg.itb
```

1. EtherType/ARP drop: baseline ping was 3/3; the `protocol arp` rule reported
   `in_hw` and produced 0/3 after neighbor flush; deletion restored 5/5.
2. IPv4 protocol drop: an `ip_proto icmp` rule reported `in_hw` and changed
   ping from 2/2 to 0/3; deletion restored consecutive replies.
3. MAC drop: exact Windows source MAC changed ping to 0/2; deletion restored
   3/3.
4. IPv4 address and L4 port: `ip_proto udp src_ip 192.168.5.100/32 dst_port
   5201` reported `in_hw`. The TCP iperf control connection was accepted but
   the UDP run received no data and terminated; after deletion, the same run
   delivered 99.5 Mbit/s with 0/25,620 datagrams lost.
5. Ordering and reuse: adding pref 20 ICMP followed by pref 10 ARP produced the
   expected tc order. Removing ARP left the neighbor reachable while ICMP
   remained blocked; removing ICMP restored 3/3. Re-adding ARP to the released
   slot again produced 0/2, and final deletion restored 3/3.
6. Replace: replacing an exact-source-MAC drop rule with a mirror rule using
   the same handle changed ping from 0/2 to 3/3 without deleting the filter.
7. Redirect: redirecting the same matched traffic to link-down `lan2` changed
   ping to 0/3; deletion restored 3/3.
8. Shared meter: an exact-source-MAC rule with `action police rate 100mbit
   burst 2m conform-exceed drop` reported `in_hw` and limited a 500 Mbit/s UDP
   offer to 99.8 Mbit/s. Replacing the same cookie at 200 Mbit/s yielded
   198 Mbit/s; deletion restored 498 Mbit/s with zero loss, and re-adding the
   100 Mbit/s rule yielded 104 Mbit/s including the initial burst. The meter
   index was therefore programmed, replaced, released, and reused by real
   traffic.
9. Full IPv6 and multi-template AND: Windows link-local ICMPv6 was 3/3 before
   filtering. A four-template rule containing the correct destination and
   next-header but source `fe80::1` still passed 3/3. Replacing the source with
   the exact Windows address produced 0/3, and deletion restored 3/3. A
   destination-only IPv6 rule also produced 0/3.
10. HSB/parser evidence: an 800 Mbit/s IPv6 UDP stream was captured through the
    standard ethtool register dump with EtherType `0x86dd`, IPv6 parser type 2,
    source port 7, native low-32-bit SIP/DIP, and all programmed UDF selector
    values. This was used to verify the corrected template 2/3 layout.
11. L4 range table: with a UDP destination range 5200-5300, a datagram to 5400
    arrived (`7` bytes) while a datagram to 5250 timed out and left a zero-byte
    receive file. Deleting the rule made the same 5250 datagram arrive (`7`
    bytes); IPv4 ping remained 2/2 afterwards.
12. VLAN match: the Windows Realtek peer was configured to transmit tagged
    VID 100 traffic into a VLAN-aware bridge. A VID 100/IPv4/ICMP drop rule
    reported `in_hw`, dropped all four probes, and `tc -s` reported exactly
    four hardware packets. Deleting the rule restored 2/2 replies. Single
    C-tag VID 100/PCP 5/IPv6 and stacked S-tag VID 200 + C-tag VID 100/PCP 3
    rules also passed TCAM/action readback.
13. Logging-counter lifecycle: an IPv4 ICMP drop rule blocked exactly five
    probes and `tc -s` reported `Sent hardware 5 pkt`; a second read without
    traffic remained at five while `used` aged by two seconds. Deleting it and
    adding a pass rule reused counter 0 from zero, then reported exactly two
    successful probes. The reset register had to be pulsed high then low;
    leaving its bit high holds that counter at zero.
14. Multi-template statistics and priority action: the four-template exact
    IPv6 source+destination ICMPv6 rule reported exactly three dropped hardware
    packets. After booting a kernel with `act_skbedit`, `skbedit priority 5
    pipe` followed by `pass` reported `in_hw`; three successful probes produced
    exactly three hardware packets. Action-table and counter-register readback
    passed in both cases.
15. DSCP remark: two baseline Windows-to-Mac routed ICMP probes arrived on
    `en8` with TOS `0x00`. A `skip_sw` IPv4 rule using `pedit ex munge ip
    dsfield set 0xb8 retain 0xfc`, IPv4-header csum, and pass reported `in_hw`.
    Three subsequent probes all arrived with TOS `0xb8`, all replies returned,
    and `tc -s` reported exactly three hardware packets.
16. VLAN modify: with tagged VID 100 ingress and bridge VLAN 200 prepared, a
    `vlan modify id 200 protocol 802.1Q priority 3` rule reported `in_hw`.
    Three probes increased `br0.200` RX by three, learned the Windows neighbor
    on `br0.200`, and produced exactly three hardware packets in `tc -s`.
17. Factory-style S-VLAN CPU transport: the switch now assigns private
    SVID59..62 to RTL ports 4..7 and keeps customer C-tags as the inner tag.
    An ARP `action trap` rule reported `in_hw`, counted exactly one hardware
    packet, retained the correct SVID62 source identity, and the three probes
    all returned. An IPv4 `vlan push id 100 priority 3` rule produced the
    captured CPU-link order SVID62 followed by C-VID100/PCP3 and counted exactly
    one packet; deleting it restored untagged traffic immediately. A tagged
    VID100 `vlan pop` rule programmed and counted real ingress frames, but the
    available one-peer fixture lacked VLAN100 CPU membership, so endpoint
    delivery for pop remains a separate acceptance item.

On the final combined FIT, standalone traffic passed 3/3, the ARP trap counted
exactly one hardware packet while all three probes returned, and a bridge
join/leave cycle restored standalone traffic at 3/3. No RTL8372N error,
warning, Oops, or BUG appeared.

The mirror rule preserved the original path at 5/5. A second live LAN peer is
still required to prove the mirrored copy at the destination endpoint.

## Deliberate limits

- The ASIC has 32 logging counters. Stats-enabled non-police rules consume one
  counter each; use `hw_stats disabled` when only TCAM capacity is required.
  The selected 32-bit packet mode has no byte counter, so `tc -s` reports exact
  packets and `lastused` with zero bytes.
- ACL logging and policing share the same action selector. A police rule keeps
  the shared meter and therefore cannot expose a simultaneous logging counter.
- SVID59..62 are private CPU-link tags and are rejected as customer VLANs.
  The trap action assigns the ingress port's private SVID and redirects through
  normal CPU8 service-port egress; the special tagless external-CPU exception
  path is intentionally not used.
- VLAN push and modify have endpoint/capture A/B. VLAN pop has hardware
  programming and packet-hit evidence; its final endpoint A/B needs a tagged
  peer plus VLAN membership on the CPU port.
