# Fast Statistics and Aging Research

## Status

Hardware fast-entry packet, byte, and age-derived last-used statistics are
implemented and board-validated. The first 1,024 hardware directions own an
independent 10-bit statistics ID, read their two 64-bit SDT29 counters, and
return exact deltas through `FLOW_CLS_STATS` with
`FLOW_ACTION_HW_STATS_DELAYED`. The hardware has 4,096 independent IKEY and
age entries, so later directions remain offloaded with the response's
statistics-enable bit clear: packet and byte deltas stay zero, while the real
SE age bit refreshes `lastused`.

The age bit is read and cleared twice, matching `np.ko`; activity observed in
either read moves `lastused` to the current `jiffies`. Rule insertion and a
stats read without traffic cannot manufacture activity. Linux flowtable
lifetime remains the owner of expiration.

This note preserves the useful evidence from Pi session
`01a039cf-767f-7157-ab4c-1c9563470913` without retaining its experimental
kernel code or debugfs register-write interfaces.

## Superseded Probe Result

The existing SDT14/ZCAM/IKEY path continued to deliver the canonical UDP
SNAT/DNAT exchange at `10/10`. Repeated tests with traffic in flight found:

- the candidate SDT29 packet and byte reads remained zero;
- the candidate age words remained idle;
- full indirect-ERAM snapshots did not change after thousands of packets.

Those reads did not reproduce the vendor 64-bit SDT access contract and are no
longer evidence that the counters are inactive. They remain useful evidence
that a successful ZCAM lookup alone does not validate a proposed counter
reader.

## Recovered Vendor Contract

The session used the standalone `out/sesdump.c` `/dev/mem` probe on the vendor
runtime and observed SMMU0/ALG activity around session creation. Focused
analysis of `np.ko` and its `switch.ko` caller established the contract:

- `zte_api_fast_l3_session_flow_statis(flow_id, &packets, &bytes)` reaches
  `stat_flow_stat_get()` and reads SDT29 entries `2 * flow_id` and
  `2 * flow_id + 1`;
- SDT29 contains 2048 64-bit entries at base block `0x9081`, providing 1024
  packet/byte counter pairs;
- a 64-bit entry uses SMMU0 command `0x08000000`, address
  `(0x9081 << 7) + entry * 64`, and the second 64 bits of the four-word read
  result; read-clear would additionally set bit 30 but is unnecessary here;
- `fast_hashinfo_set()` enables statistics and encodes the low ten bits of the
  flow ID when the response requests statistics;
- the mainline response already had the statistics-enable bit; replacing the
  shared IDs with allocated per-entry IDs activates independent counters;
- session creation writes an IKEY response whose first word appeared as the
  post-SNAT source port followed by constant flags `0x0002`;
- the earlier candidate addresses `0x484180`/`0x4841c0` are exactly the
  packet/byte pair for the old shared flow ID 2.

Some early bulk reads returned stale data because the probe did not initially
observe the done transition correctly. Every address, entry layout, and
parser command must therefore be captured again with before/after transitions
and independently checked before it is used by the driver.

## Rejected Experimental Implementation

The discarded implementation had several properties that made it unsuitable
for the production driver:

- fixed statistics IDs `2` and `302` were shared by every flow in a
  direction, so they could not provide per-flow counters;
- reading debugfs statistics mutated the TC delta baseline;
- reading debugfs age state consumed the read-clear bit;
- a periodic worker polled a side table known not to receive hits;
- raw ZREG writes, full ERAM scans, footer overrides, and parser experiments
  were exposed from the network driver;
- speculative parser/ZREG entries had no complete allocation, rollback, or
  deletion lifecycle.

The standalone `out/sesdump.c` probe and the modified vendor `np.ko.i64` IDA
database are retained as research artifacts. They are not part of the kernel
ABI or the accepted FIT.

## Mainline Implementation

`zx279133-offload.c` allocates one statistics ID per hardware rule alongside
its ZCAM, IKEY, and age resources. It snapshots the raw packet and byte totals
before publishing the ZCAM entry, preventing a reused ID from inheriting the
previous rule's counts. Stats reads subtract the saved raw totals, update the
baseline once, and refresh `lastused` only after real counter movement. Delete,
rollback, flush, and cleanup release the ID through the same serialized flow
lifecycle. No debugfs ABI, periodic poller, raw register-write interface, or
read-clear side effect was added.

## Board Acceptance

The FIT was loaded once, then Linux ran `reboot -f` and U-Boot ran only
`bootm`. Two `skip_sw` UDP flower rules reported `in_hw_count 1` and zero
baseline statistics.

- Windows sent 16,512 matching LAN-to-WAN packets. Hardware reported exactly
  16,512 packets and 25,065,216 bytes, or 1,518 bytes per packet including the
  Ethernet FCS.
- After 22 idle seconds, TC reported `used 22 sec` without changing either
  counter. A second burst of 8,256 packets produced exact cumulative totals of
  24,768 packets and 37,597,824 bytes and refreshed the used time.
- The independent WAN-to-LAN rule then counted exactly 16,512 packets and
  25,065,216 bytes without changing the LAN-to-WAN total.
- Deleting both counted rules and adding rules for a new port reused the
  hardware IDs but correctly reported zero packets and bytes, proving that
  stale raw totals do not leak across lifetimes.

The final `W=1` FIT was then loaded and accepted again through `reboot -f`
followed by direct `bootm`. Both directions independently counted exactly
8,256 packets and 12,532,608 bytes. An ordinary nftables TCP NAT connection
also reached `[HW_OFFLOAD]` and transferred 1,477,378,048 payload bytes in
5.000235 seconds. The kernel log remained free of BUG, Oops, WARNING, WANID,
and SMMU failures.

The completed lifecycle acceptance used the same FIT and real SDT29 stats:

- one established UDP flow remained `[HW_OFFLOAD]` at 5, 30, and 60 seconds
  while 115,587 request/reply packets crossed each direction, proving that
  counter-driven `lastused` refresh survives two 30-second flowtable timeout
  periods;
- a separately timed flow remained in hardware at idle seconds 0, 20, and 28,
  lost only the hardware marker by second 34, and retained its conntrack entry;
- 100 consecutive add, zero-stats read, delete, and counter-ID reuse cycles
  completed with no programming, stats, deletion, or residual-rule failure;
- 256 concurrent NAT connections installed 512 independent hardware
  directions. Eight distributed connections each counted exactly 825 packets
  and 1,252,350 hardware bytes in both directions, while eight untouched
  connections remained at zero. All 256 connections then deleted without a
  residual rule.

Both links remained up and the kernel log stayed free of BUG, Oops, WARNING,
WANID, and SMMU failures. `udp-echo.c`, `flow-stats-reuse.sh`, and
`flow-stats-capacity.sh` preserve the repeatable harnesses.

The accepted FIT is `/Volumes/code/zx279133/out/sr1010-zxdbg.itb`, 5,975,852
bytes, SHA256
`8c8eeb65b8cb059eddb594f8369d990fa8178d9edcb434aaa322ec753dc49aa7`.

The large-table acceptance then installed 2,048 concurrent NAT connections,
or all 4,096 IKEY/age entries. Every direction reported `in_hw_count 1`; the
2,049th connection failed cleanly with `-ENOSPC`. A direction beyond the
1,024-counter pool forwarded exactly 100 packets while retaining zero
packet/byte totals, and its TC `last_used` value refreshed from the SE age bit
in both directions. Deleting all 2,048 connections left no rule behind.

`flow-zcam-collision.sh` separately installed 32 bidirectional connections
whose primary CRC16 locations deliberately collide. All 64 directions reached
hardware through the remaining ZCAM candidates.
