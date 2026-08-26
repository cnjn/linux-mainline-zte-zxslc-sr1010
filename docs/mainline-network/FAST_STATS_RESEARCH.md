# Fast Statistics and Aging Research

## Status

Hardware fast-entry hit counters and hardware-assisted flow aging are not
implemented. The accepted mainline backend continues to provide rule
programming and packet forwarding only; `FLOW_CLS_STATS` must not claim
hardware packet, byte, or last-used updates until the lookup path that drives
the vendor side tables has been reproduced and validated.

The production driver therefore returns `-EOPNOTSUPP` for `FLOW_CLS_STATS`.
The former synthetic callback, which reported the rule insertion time as
`lastused`, has been removed. The age index carried by the validated ZCAM
response format is still allocated and released with each rule; this is
resource ownership, not a claim that the inactive age side table is reporting
hits.

This note preserves the useful evidence from Pi session
`01a039cf-767f-7157-ab4c-1c9563470913` without retaining its experimental
kernel code or debugfs register-write interfaces.

## Confirmed Mainline Result

The existing SDT14/ZCAM/IKEY path continued to deliver the canonical UDP
SNAT/DNAT exchange at `10/10`. Repeated tests with traffic in flight found:

- the candidate SDT29 packet and byte reads remained zero;
- the candidate age words remained idle;
- full indirect-ERAM snapshots did not change after thousands of packets.

The negative result is important: successful ZCAM forwarding is not evidence
that the vendor statistics or aging side effects are active. No counter or age
reader should be merged on top of the current lookup path alone.

## Vendor Observations to Reproduce

The session used the standalone `out/sesdump.c` `/dev/mem` probe on the vendor
runtime and observed SMMU0/ALG activity around session creation. The following
are useful leads, not yet a mainline hardware contract:

- the vendor statistics call chain reaches `stat_flow_stat_get()` and
  `se_stat_table_get()` for SDT29;
- session creation writes an IKEY response whose first word appeared as the
  post-SNAT source port followed by constant flags `0x0002`;
- command snooping observed per-direction polling in the indirect ERAM area,
  including candidate addresses `0x484180`/`0x4841c0` and
  `0x48d780`/`0x48d7c0`;
- age probes used a read-clear command with bit 30 set;
- the vendor lookup appeared to program parser/multi-hash state in addition
  to ZCAM and IKEY state.

Some early bulk reads returned stale data because the probe did not initially
observe the done transition correctly. Every address, entry layout, and
parser command must therefore be captured again with before/after transitions
and independently checked before it is used by the driver.

## Rejected Experimental Implementation

The discarded implementation had several properties that made it unsuitable
for the production driver:

- fixed statistics IDs `2` and `302` were shared by every flow in a
  direction, so they could not provide per-flow counters;
- the current lookup did not update those counters or age words;
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

## Next Acceptance Gates

Future work should start by capturing the exact vendor parser-session write
sequence, including allocation and teardown, before changing the mainline
flow response. Completion requires all of the following on a cold RAM boot:

1. Two simultaneous flows report independent nonzero packet and byte deltas.
2. Re-reading one flow does not consume or alter another flow's delta.
3. Matching traffic refreshes `lastused`; an active flow does not expire.
4. An idle flow expires and its parser, response, ZCAM, counter, and age
   resources are all released.
5. UDP and TCP matching packets, reverse delivery, deletion, and throughput
   are accepted independently.
6. Debugfs contains no raw production-register write interface.
