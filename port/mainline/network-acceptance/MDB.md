# RTL8372N MDB Offload

This gate covers Linux bridge MDB synchronization into the RTL8372N L2
multicast table. It does not enable the switch's autonomous IGMP/MLD engine or
claim routed multicast offload.

## Hardware contract

The RTL8372N DAL in `lotusmomo/airoha_sdk` commit
`32b5aa356c2406edef8806aaf75451ff0a5286f2` matches the factory
`rtl8373_switch.ko` symbols and the register/table encoding recovered from that
module. The relevant source is:

- `private/ether/en7512/rtl8372n/l2.h`
- `private/ether/en7512/rtl8372n/dal/rtl8373/dal_rtl8373_lut.c`
- `private/ether/en7512/rtl8372n/dal/rtl8373/rtl8373_tableField_list.c`
- `private/ether/en7512/rtl8372n/dal/rtl8373/rtl8373_reg_definition.h`

The Linux integration model follows `perceival/openwrt-flint3` commit
`75a835fdade7ede209ad1f390080930e1a476bd1`, especially
`package/kernel/rtl837x` and its generic `dsa_tag_8021q` bridge glue. The DAL
remains the register-level hardware contract; the implementation in this tree
exposes those capabilities through standard DSA, bridge, tc, and ethtool
interfaces.

The mainline driver uses the same three-word L2 multicast entry:

- word 0 and word 1 hold the 48-bit destination MAC;
- word 1 bits 27:16 hold VID/FID, bit 29 selects IVL/SVL, and bits 31:30
  hold member-port bits 1:0;
- word 2 bits 7:0 hold member-port bits 9:2;
- word 2 bits 15:8 are `igmp_index` and bit 16 is `igmp_asic`; Linux-managed
  MDB entries leave both fields zero.

The table engine is at `0x5cac`/`0x5cb0`, with write data at
`0x5cb8..0x5cc0` and read data at `0x5ccc..0x5cd4`. Removing the final member
uses the SDK's address-based `ENTRY_CLR` operation rather than leaving a
zero-member multicast entry in the LUT.

The SR1010 DSA-to-physical port mapping is:

| DSA interface | RTL port |
| --- | ---: |
| `lan1` | 7 |
| `lan2` | 6 |
| `lan3` | 5 |
| `lan4` | 4 |
| CPU conduit | 8 |

## Current forwarding boundary

The driver uses the same S-VLAN CPU-link model as the factory switch stack.
RTL ports 4..7 receive private default SVID59..62, while CPU8 is the sole
S-VLAN service port. The dedicated DSA tagger removes only that outer private
S-tag and derives the source port from it. A customer C-tag remains as the
inner tag, so VLAN-aware bridges and non-zero-VID MDB lookups do not alias the
CPU transport identity.

MDB entries with VID 0 use SVL/FID 1. Non-zero customer VIDs use IVL with the
VID as FID. CPU8 membership is driven by the normal DSA host-MDB callbacks,
rather than being forced into every user-port entry. This permits autonomous
LAN-to-LAN multicast forwarding while retaining a CPU copy only when Linux has
joined the group.

## Diagnostic utility

`mdb-ctl` is a small rtnetlink client included in the diagnostic initramfs. It
does not expose driver registers or a private kernel ABI.

```sh
busybox brctl addbr br0
busybox brctl stp br0 off
ip link set lan1 up
ip link set lan2 up
busybox brctl addif br0 lan1
busybox brctl addif br0 lan2
ip link set br0 up

mdb-ctl add br0 lan1 239.1.1.1
mdb-ctl add br0 lan2 239.1.1.1
mdb-ctl dump br0
mdb-ctl del br0 lan1 239.1.1.1
mdb-ctl del br0 lan2 239.1.1.1
```

IPv6 uses the same interface:

```sh
mdb-ctl add br0 lan1 ff3e::1234
mdb-ctl del br0 lan1 ff3e::1234
```

## Board acceptance, 2026-09-02 (pre-generic-tagger baseline)

Final FIT:

```text
e056918f3d27b3095756b07477631db4cd2352aa37b648a7af3fe86dccf53827  sr1010-zxdbg.itb
```

This older private-VID FIT passed the following MDB table tests. Its software
bridge and VID-100 limitations below are historical and are superseded by the
current S-VLAN transport design:

- IPv4 and IPv6 user-port entries reported `offload=1 failed=0`.
- Two ports sharing one group produced two Linux MDB objects backed by one
  read-modify-written RTL member mask. Deleting one retained the other;
  deleting the final user member cleared the LUT entry and a new add reused it.
- A Windows sender on `lan1` transmitted 50 packets to a group whose only
  user member was `lan2`. CPU-visible `lan1` RX rose from 125 to 188, proving
  the final CPU8 fallback prevents the hardware hit from cutting off the
  software bridge.
- 256 distinct IPv4 groups added successfully, all 256 appeared in the dump,
  all 256 deleted successfully, and zero remained.
- A VID 100 MDB stayed in software (`offload=0`); the driver did not falsely
  advertise an inner-customer-VID hardware lookup.
- With active MDB entries, removing `lan2` and then `lan1` from the bridge
  removed their memberships. The switch and tagger modules unloaded and
  reloaded cleanly. Afterwards `lan1` pinged Windows `192.168.5.100` 3/3 with
  zero packet loss.
- No `MDB failed`, `BUG`, `Oops`, `WARNING`, or teardown error was logged.

An intermediate diagnostic build temporarily omitted the production CPU8
fallback to isolate the RTL lookup effect. From the same Windows sender:

- 100 packets to an unknown group changed CPU-visible RX from 234 to 337;
- 100 packets to a known group containing only `lan2` changed RX from 337 to
  339;
- adding CPU8 to the same group changed RX from 339 to 457;
- deleting CPU8 and sending another 100 packets left RX at 457;
- deleting the final LUT member restored unknown flooding, changing RX from
  461 to 562 for 100 packets.

This A/B proves real RTL8372N table hits and member-mask behavior. The
intermediate no-CPU mask is evidence only; it is not the production forwarding
policy.

## DSA tagger acceptance, 2026-09-03 to 2026-09-04

The earlier generic-tagger FIT
`f11d761bbd1d7b5dfe7c4a601d102969ee96a973bda69a28725e57bfba4e4cff`
passed standalone receive, VLAN-unaware bridge receive, and a VLAN-aware bridge
with `lan1` as untagged/PVID 100 and `lan2` as tagged VID 100. The Windows peer
at `192.168.5.100` replied 5/5 through the VLAN-aware bridge. Dynamic FDB
learning showed the customer VID plus generic transport VIDs, bridge ageing was
accepted, the fast-age transition removed the dynamic entry, and bridge teardown
restored standalone `lan1` traffic. The later factory-style S-VLAN tagger kept
the same source-port mapping, passed standalone traffic, and passed a bridge
join/leave cycle followed by 3/3 standalone probes. The transport change does
not alter the RTL multicast LUT or Linux MDB ownership model.

On the current QoS/ACL build, real endpoint reports also closed the dynamic
snooping lifecycle. A Windows `UdpClient.JoinMulticastGroup()` on `lan1`
created `239.1.2.3` as `temp proto kernel offload`; closing the socket removed
the entry. The IPv6 equivalent created and removed `ff3e::1234` with the same
`offload` state. Thus Linux bridge IGMP/MLD parsing, dynamic MDB notification,
RTL table programming, and leave deletion form one automatic standard-API
path; no private switch daemon is required.

The RTL autonomous IGMP/MLD group engine remains disabled deliberately. It
would learn a second private table invisible to Linux and race bridge MDB
lifetime. Direct LAN-to-LAN multicast replication still needs a second live
LAN endpoint for an endpoint-level proof; the earlier no-CPU-mask A/B remains
the hardware table-hit evidence.
