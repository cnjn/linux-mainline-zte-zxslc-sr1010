# RTL8372N DCB and qdisc hardware offload

This gate covers the RTL8372N ingress-priority and egress-scheduler blocks
through standard Linux interfaces. It is independent of NPPT routed-flow
offload and of the RTL ACL policer.

## Hardware contract

The register contract was recovered from `lotusmomo/airoha_sdk` commit
`32b5aa356c2406edef8806aaf75451ff0a5286f2`, especially
`dal_rtl8373_qos.c`, `dal_rtl8373_rate.c`, and
`rtl8373_reg_definition.h`.

- two global priority-decision profiles, selected per port;
- port default priority and a global 64-entry DSCP-to-priority map;
- PCP and DSCP trust with fixed PCP-before-DSCP precedence;
- eight internal priorities and eight egress queues per port;
- strict priority or 1..127 WFQ weight per queue;
- port and per-queue token-bucket egress rate control.

The 64-bit egress control records are addressed high word first: rate/enable
is at the DAL base address and burst is at `base + 4`. Both port and queue rate
fields use 16 kbit/s units. The published queue DAL omits the shift even though
its 20-bit field and rate-limit constants require it; board traffic confirmed
the hardware unit.

## Linux interface

- `dcb app`: per-port default priority and the global DSCP priority map;
- `dcb apptrust`: PCP, DSCP, or PCP followed by DSCP;
- `tc ets`: exactly eight bands, arbitrary priority-to-band mapping, strict
  queues, and weighted queues;
- `tc tbf`: root port shaping or a TBF child on one of the eight ETS bands.

The private CPU-link S-VLAN carries `skb->priority` in its PCP bits, so host-originated
traffic and Linux traffic-control classification reach the same RTL8372N
internal-priority path. Profile 0 remains available to CPU port 8 because the
transport contract must trust PCP. The remaining profile can be shared by any
user ports with an identical trust set; a third simultaneous trust policy is
rejected with `ENOSPC`, matching the two-profile hardware limit.

## Board acceptance, 2026-09-03

Topology: Windows `192.168.5.100` on `lan1`, router `192.168.5.1`.

1. `dcb apptrust show` returned the reset policy `pcp`. `pcp dscp` and
   DSCP-only policies round-tripped through the driver. With profile 0 occupied
   by CPU/PCP and `lan1` using PCP+DSCP, a different DSCP-only policy on
   `lan2` failed with `No space left on device`; releasing profile 1 allowed
   the same policy immediately.
2. `dcb app replace` changed default priority from 0 to 5 and DSCP 46 from 0
   to 6; filtered output returned `default-prio 5` and `46:6`. Both were then
   restored to zero.
3. An eight-band ETS qdisc accepted both eight strict queues and a mixed
   four-strict/four-WFQ configuration with normalized weights 10/20/30/40.
   Queue-map and scheduler registers are verified after every write.
4. A queue-0 TBF configured for 100 Mbit/s was latched for diagnosis and the
   software TBF child was removed. With only hardware shaping remaining,
   Windows received 96.2 Mbit/s. Changing only ETS priority 0 from queue 0 to
   unshaped queue 1 restored 197 Mbit/s. This proves the packet reached the
   programmed hardware queue, not merely the software qdisc.
5. With the production teardown path, a 100 Mbit/s queue TBF delivered
   92.1 Mbit/s with no packet loss at the endpoint; deleting the child restored
   197 Mbit/s. The diagnostic latch was never retained in source or in the
   final module.
6. Final cleanup left no qdisc, reset DCB state to PCP/default 0/DSCP 46:0,
   retained 3/3 LAN ping, and added no RTL8372N error, warning, Oops, or BUG.

## Deliberate limits

- Only two distinct apptrust policies can coexist because the ASIC has two
  decision profiles and CPU transport permanently needs a PCP-capable one.
- ETS requires all eight hardware bands; CBS, TAPRIO, ETF, and frame
  preemption are not implemented by this block.
- TBF exposes the 16-bit hardware burst field, so larger bursts are rejected.
- Per-queue packet/byte counters and qdisc stats offload are not yet exposed.
- Weighted fairness under two simultaneously backlogged hardware classes still
  needs a second live LAN sender; register programming and readback are covered,
  but one endpoint cannot establish a bandwidth ratio.
