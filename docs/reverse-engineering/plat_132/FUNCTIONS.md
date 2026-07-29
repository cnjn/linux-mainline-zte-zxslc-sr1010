# plat_132.ko Function Ledger

## Purpose

This is the per-function progress ledger. It is intentionally separate from
the durable architecture memory so that future context compression retains
high-value facts without carrying every row.

## Status Legend

- `queued`: not yet analyzed under the completion criteria.
- `in-progress`: currently being reconstructed.
- `review`: reconstruction exists and needs cross-checking.
- `complete`: evidence, semantics, side effects, and confidence are recorded.
- `blocked`: requires missing external evidence.

## Entry Template

```text
### 0x00000 name
Status: queued
Confidence: unknown
Role:
Inputs/outputs:
Globals/MMIO/callbacks:
Concurrency/ownership:
Evidence:
Open questions:
Recovered source:
```

## Initial Queue

### 0x1c3b4 init_module
Status: complete
Confidence: verified
Role: Module entry: select CPU type 133, register the PON platform driver, then
initialize NPPT only after registration succeeds.
Inputs/outputs: Returns the first nonzero status from `pon_driver_register` or
the status returned by `nppt_init`.
Globals/MMIO/callbacks: Writes `g_pon_cputype=2`; no direct MMIO access.
Concurrency/ownership: Does not unwind a successful platform-driver
registration if `nppt_init` later fails.
Evidence: IDA decompiler, 12-instruction disassembly, xrefs, and CPU-type
helper functions at `0x1dc`, `0x1f0`, and `0x204`.
Open questions: Validate exit-path ownership with `plat_cleanupModule`.
Recovered source: `recovered/plat_module.c`; detailed record:
`functions/0x1c3b4-init_module.md`.

### 0x0e8c pon_driver_register
Status: complete
Confidence: verified for call behavior; strong inference for field labels.
Role: Register static `zx_pon_driver` with the platform bus under
`__this_module` ownership.
Inputs/outputs: Returns the unchanged `__platform_driver_register` status.
Globals/MMIO/callbacks: Reads `zx_pon_driver @ 0x26438` and
`__this_module @ 0x27340`; no direct MMIO or mutable global writes.
Concurrency/ownership: Matching unregister happens after `nppt_exit` in
`plat_cleanupModule`.
Evidence: 10-instruction disassembly, direct called function, driver-object
data values, vendor strings, and separately labeled upstream 5.4 ABI reference.
Open questions: Reconstruct the full driver object and OF match table.
Recovered source: `recovered/plat_module.c`; detailed record:
`functions/0x0e8c-pon_driver_register.md`.

### 0x0580 zx_pon_probe
Status: complete
Confidence: strong inference
Role: Enumerate PON-platform OF nodes, map resources/IRQs, select PON mode,
perform reset/clock/SerDes bring-up, then register PON and NPPT IRQ handlers.
Inputs/outputs: Candidate platform-device pointer; returns 0, a propagated
initialization/IRQ error, or literal `-19` on required resource failure.
Globals/MMIO/callbacks: Initializes PON, NPPT, SerDes, XMAC PCS, GEPHY, RGMII,
PPS, IDM IRQ, low-power alias, work-mode, and LAN-routing globals.
Concurrency/ownership: No observed unwind for OF maps or a successful PON IRQ
when a later operation fails. Manual low-power aliases are mapped inside every
OF-match-loop iteration.
Evidence: IDA decompiler and xrefs, vendor compatible strings, vendor DTS, and
vendor runtime dmesg. Upstream 5.4 is used only as a labeled ABI reference for
the candidate probe argument type.
Open questions: Exact `CspGetPortInfo` contract, complete OF match table,
sysctrl/efuse field semantics, ioremap-flag meaning, and resource cleanup.
Recovered source: `recovered/plat_probe.c`; detailed record:
`functions/0x0580-zx_pon_probe.md`.

### 0x0308 zx_pon_remove
Status: complete
Confidence: verified
Role: Release PON and NPPT top-level IRQ registrations during platform removal.
Inputs/outputs: Platform-device argument is unused; always returns 0.
Globals/MMIO/callbacks: Indirectly frees `g_pon_irq` and `g_nppt_irq` with the
shared `pon_int_info` dev-id. No MMIO access.
Concurrency/ownership: Does not directly clean IDM IRQs, mappings, or data
plane state; module cleanup calls `nppt_exit` before this callback.
Evidence: 7-instruction disassembly and direct decompilation of both unregister
helpers.
Open questions: Assign remaining teardown responsibilities after `nppt_exit`
and IDM teardown reconstruction.
Recovered source: `recovered/plat_probe.c`; detailed record:
`functions/0x0308-zx_pon_remove.md`.

### 0x11a50 nppt_init
Status: complete
Confidence: verified
Role: Initialize SIPC, GREG, SMAC, and IDM in sequence and aggregate all
statuses with bitwise OR.
Inputs/outputs: Takes no semantic arguments; returns the 32-bit OR of all four
callee status values.
Globals/MMIO/callbacks: No direct global/MMIO access beyond logging; callees
perform the hardware work.
Concurrency/ownership: Runs all stages despite earlier failure and does not
perform rollback.
Evidence: 22-instruction disassembly, direct callee decompilation, module-exit
xrefs, and contiguous vendor runtime logs.
Open questions: Individual SMAC/IDM error sets and init-failure cleanup policy.
Recovered source: `recovered/plat_module.c`; detailed record:
`functions/0x11a50-nppt_init.md`.

### 0x16a0 register_pon_int
Status: complete
Confidence: verified
Role: Register the PON top-level hard IRQ handler.
Inputs/outputs: Returns a negative `request_threaded_irq` status, otherwise 0.
Globals/MMIO/callbacks: Uses `g_pon_irq`, `zx_pon_int`, and shared
`pon_int_info`; no direct MMIO access.
Concurrency/ownership: Paired with `unregister_pon_int`; called before NPPT IRQ
registration during probe.
Evidence: 25-instruction disassembly, paired unregister decompilation, xrefs,
and vendor runtime interrupt label.
Open questions: External callback registration order and lifetime for the shared
`pon_int_info` context slot.
Recovered source: `recovered/plat_irq.c`; detailed record:
`functions/0x16a0-register_pon_int.md`.

### 0x1710 register_nppt_int
Status: complete
Confidence: verified
Role: Register the NPPT top-level hard IRQ handler.
Inputs/outputs: Returns a negative `request_threaded_irq` status, otherwise 0.
Globals/MMIO/callbacks: Uses `g_nppt_irq`, `zx_nppt_int`, and shared
`pon_int_info`; no direct MMIO access.
Concurrency/ownership: Paired with `unregister_nppt_int`; called only after PON
IRQ registration succeeds.
Evidence: 25-instruction disassembly, paired unregister decompilation, xrefs,
and vendor runtime interrupt label.
Open questions: External callback lifetime for the shared `pon_int_info` context
slot.
Recovered source: `recovered/plat_irq.c`; detailed record:
`functions/0x1710-register_nppt_int.md`.

### 0x12bc zx_pon_int
Status: complete
Confidence: verified for control flow, callback dispatch, and MMIO offsets.
Role: Top-level PON hard-IRQ dispatcher and DGi recovery trigger.
Inputs/outputs: Ignores the numeric IRQ, uses `dev_id` as a pointer to the
shared `pon_int_info` context slot, and always returns 1.
Globals/MMIO/callbacks: Reads `pon_base + 0x40` and `+0x44`, dispatches GPON,
XGPON, EPON, XEPON, XEDPON, LP, and low-power callback slots, updates PON state
flags, and executes mode-specific DGi MMIO writes.
Concurrency/ownership: Callback slots and their shared context are mutable and
not visibly synchronized here. The low-power callback is invoked without a null
test when its bit is active.
Evidence: 191-instruction disassembly; callback registration helpers;
`pon_int_enable`; ONU/LLID state helpers; DGi timer and counter xrefs.
Open questions: Register-field names, DGi MMIO semantics, and external callback
lifetime.
Recovered source: `recovered/plat_irq.c`; detailed record:
`functions/0x12bc-zx_pon_int.md`.

### 0x0ed4 zx_nppt_int
Status: complete
Confidence: verified for control flow, callback dispatch, and MMIO offsets.
Role: Top-level NPPT hard-IRQ dispatcher for OAM, PTP, and PTP-stamp events.
Inputs/outputs: Ignores the numeric IRQ; uses `dev_id` only for PTP stamp;
always returns 1.
Globals/MMIO/callbacks: Reads `nppt_base + 0x0` and `+0x4`, dispatches three
callback slots, sets `soam_alarm_flag`, and does 11 OAM-path volatile reads.
Concurrency/ownership: Callback slots are mutable and no synchronization is
visible. OAM/PTP callbacks receive null arguments while PTP stamp receives the
shared context slot's current value.
Evidence: 52-instruction disassembly; NPPT enable and callback-registration
helpers; callback/flag xrefs.
Open questions: NPPT register names, OAM read semantics, and external callback
lifetime.
Recovered source: `recovered/plat_irq.c`; detailed record:
`functions/0x0ed4-zx_nppt_int.md`.

### 0x14ff4 idm_init
Status: complete
Confidence: verified control flow and raw MMIO; strong inference for queue/FIFO
layouts.
Role: Allocate and populate IDM buffer/ring state, program IDM registers,
configure IDM IRQs, and hand off to the CPU network subsystem.
Inputs/outputs: No semantic argument; returns `-1` on local failure and 0 after
calling `cpu_net_init` regardless of that callee's status.
Globals/MMIO/callbacks: Uses the vendor reserved-memory exports, per-CPU free
data, 24 RX/4 TX queue arrays, FIFO/cache state, `nppt_base + 0x280000` IDM
registers, `idm_ops`, and CPU-net callback registration data.
Concurrency/ownership: Zeros lock words and per-CPU counters. Failure paths do
not free partial allocations or unwind hardware/IRQ state; jumbo FIFO and cache
allocation results are not checked.
Evidence: 924-instruction disassembly; direct callee decompilation; vendor
runtime IDM allocation, cache, ring, IRQ, and netdev logs.
Open questions: Hardware field names, exact descriptor ownership, extra 0x800
capacity margin, and lifecycle cleanup.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x14ff4-idm_init.md`.

### 0x14d88 idm_cfg_int
Status: complete
Confidence: verified request flow and target-CPU values; strong inference for
affinity-bitmap layout.
Role: Program IDM interrupt words, request four IDM hard IRQs, and set CPU
affinity hints.
Inputs/outputs: No semantic arguments; returns the first negative request status
or 0. Affinity-hint statuses are ignored.
Globals/MMIO/callbacks: Writes `nppt_base + 0x280044..0x280050`, requests
`g_idm_irq[0..3]` with four IDM handlers, and updates four adjacent target-CPU
globals.
Concurrency/ownership: Later request failure leaks earlier successful IRQ
registrations. No teardown appears here.
Evidence: 153-instruction disassembly, callers/xrefs, runtime IRQ values, and
cross-function CPU-bitmap use.
Open questions: IDM field names, CPU-bitmap import declaration, and IRQ teardown.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x14d88-idm_cfg_int.md`.

### 0x13a78 idm_int_enable
Status: complete
Confidence: verified
Role: Clear selected source bits from the IDM interrupt mask under local IRQ and
raw-lock protection.
Inputs/outputs: Takes a source-bit mask; no semantically used direct return.
Globals/MMIO/callbacks: Updates `idm_int_mask` and writes
`nppt_base + 0x280040`; exposed through the IDM ops table.
Concurrency/ownership: Saves/restores local IRQ state and releases
`idm_lock_int` with byte-width release semantics.
Evidence: 23-instruction disassembly, paired disable helper, IDM handler xrefs,
and raw ops-table data.
Open questions: Lock/type ABI, re-enable callers, and hardware mask bits.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x13a78-idm_int_enable.md`.

### 0x13ad8 idm_int_disable
Status: complete
Confidence: verified
Role: Set selected source bits in the IDM interrupt mask under local IRQ and
raw-lock protection.
Inputs/outputs: Takes a source-bit mask; no semantically used direct return.
Globals/MMIO/callbacks: Updates `idm_int_mask` and writes
`nppt_base + 0x280040`; called by every IDM hard IRQ handler.
Concurrency/ownership: Saves/restores local IRQ state and releases
`idm_lock_int` with byte-width release semantics.
Evidence: 23-instruction disassembly, paired enable helper, and direct handler
callers.
Open questions: Re-enable paths, lock/type ABI, and hardware mask bits.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x13ad8-idm_int_disable.md`.

### 0x13bb0 idm_cpu_int
Status: complete
Confidence: verified
Role: Mask the IDM CPU source and hand source index 0 to CPU-net NAPI dispatch.
Inputs/outputs: Ignores hard-IRQ arguments and returns 1.
Globals/MMIO/callbacks: Reads `idm_info + 0x0` through the disable helper and
calls `cpu_net_int(0)`.
Concurrency/ownership: Performs mask-before-NAPI handoff; no direct hardware
acknowledgement in this wrapper.
Evidence: 10-instruction disassembly, registration xrefs, and `cpu_net_int`
decompilation.
Open questions: NAPI layout, poll completion, and re-enable path.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x13bb0-idm_cpu_int.md`.

### 0x13b88 idm_wifi_int
Status: complete
Confidence: verified
Role: Mask the second IDM source and hand source index 1 to CPU-net NAPI
dispatch.
Inputs/outputs: Ignores hard-IRQ arguments and returns 1.
Globals/MMIO/callbacks: Reads `idm_info + 0x4` through the disable helper and
calls `cpu_net_int(1)`.
Concurrency/ownership: Performs mask-before-NAPI handoff; no direct hardware
acknowledgement in this wrapper.
Evidence: 10-instruction body, registration xrefs, and shared helper analysis.
Open questions: Source-1 hardware meaning, poll completion, and re-enable path.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x13b88-idm_wifi_int.md`.

### 0x13b60 idm_rls_int
Status: complete
Confidence: verified
Role: Mask the fourth IDM source word and hand source index 3 to CPU-net NAPI
dispatch.
Inputs/outputs: Ignores hard-IRQ arguments and returns 1.
Globals/MMIO/callbacks: Reads `idm_info + 0xc` through the disable helper and
calls `cpu_net_int(3)`.
Concurrency/ownership: Performs mask-before-NAPI handoff; no direct hardware
acknowledgement in this wrapper.
Evidence: 10-instruction body, registration xrefs, and shared helper analysis.
Open questions: Source-3 hardware meaning, poll completion, and re-enable path.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x13b60-idm_rls_int.md`.

### 0x13b38 idm_all_int
Status: complete
Confidence: verified
Role: Mask the third IDM source word and hand source index 2 to CPU-net NAPI
dispatch.
Inputs/outputs: Ignores hard-IRQ arguments and returns 1.
Globals/MMIO/callbacks: Reads `idm_info + 0x8` through the disable helper and
calls `cpu_net_int(2)`.
Concurrency/ownership: Performs mask-before-NAPI handoff; no direct hardware
acknowledgement in this wrapper.
Evidence: 10-instruction body, registration xrefs, and shared helper analysis.
Open questions: Source-2 hardware meaning, poll completion, and re-enable path.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x13b38-idm_all_int.md`.

### 0x13088 idm_get_cpu_rx_qc
Status: complete
Confidence: verified
Role: Return a selected RX queue state entry for the IDM ops interface.
Inputs/outputs: Takes an unchecked index and returns `&idm_rx_q[index]` with a
16-byte stride.
Globals/MMIO/callbacks: Reads no MMIO; exposes the global RX queue array through
the IDM ops table.
Concurrency/ownership: No bounds checking or locking; external caller owns
index validity and synchronization.
Evidence: Five-instruction disassembly, ops-table entry, and idm-init layout.
Open questions: Full field names and external index contract.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x13088-idm_get_cpu_rx_qc.md`.

### 0x1309c idm_get_cpu_tx_q
Status: complete
Confidence: verified
Role: Return a selected TX queue state entry for the IDM ops interface.
Inputs/outputs: Returns null for indices above 3, otherwise `&idm_tx_q[index]`
with a 40-byte stride.
Globals/MMIO/callbacks: Reads no MMIO; exposes the global TX queue array through
the IDM ops table.
Concurrency/ownership: Bounds checking only; no locking or ownership transfer.
Evidence: Ten-instruction disassembly, ops-table entry, and idm-init layout.
Open questions: Full field names and external queue-user contract.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x1309c-idm_get_cpu_tx_q.md`.

### 0x137fc idm_get_cpu_rx_cnt
Status: complete
Confidence: verified register arithmetic and bit assembly; field meaning unknown.
Role: Read an IDM RX-related count from packed or direct raw register words.
Inputs/outputs: Takes an unchecked index; returns a reconstructed 32-bit value
for 0 through 7 or a raw register word for larger values.
Globals/MMIO/callbacks: Reads `nppt_base + 0x280000` offsets; exposed through
the IDM ops table.
Concurrency/ownership: No lock/latch/retry around paired reads.
Evidence: 26-instruction disassembly and ops-table entry.
Open questions: Counter-to-index mapping and split-field hardware names.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x137fc-idm_get_cpu_rx_cnt.md`.

### 0x13864 idm_get_tx_done
Status: complete
Confidence: verified register arithmetic and return truncation; field meaning
unknown.
Role: Return the low 16 bits of a selected TX-related raw IDM register word.
Inputs/outputs: Special-cases index 0 at offset `0x84`; nonzero indices use
`4 * ((index + 0x2a) & 0x3fffffff)`; returns a 16-bit value.
Globals/MMIO/callbacks: Reads `nppt_base + 0x280000` registers; exposed through
the IDM ops table.
Concurrency/ownership: No range check, lock, or snapshot mechanism.
Evidence: 13-instruction disassembly and ops-table entry.
Open questions: Valid index range and completion-field semantics.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x13864-idm_get_tx_done.md`.

### 0x138f8 idm_get_reorder_rls
Status: complete. IDM ops-table release-count reader for queues 0/1 and
CPU-133/129 queue 2. Record: `functions/0x138f8-idm_get_reorder_rls.md`.

### 0x14144 idm_rx_refill0
Status: complete
Confidence: verified control flow and staging arithmetic; strong inference for
staging/status field labels.
Role: Reuse or allocate an RX buffer and stage new physical buffer addresses for
per-CPU batched refill.
Inputs/outputs: Takes old-buffer value, pool, and reuse flag; returns 0 on
success/reuse and -1 on allocation failure even if it reuses an old buffer.
Globals/MMIO/callbacks: Uses `idm_status`, per-CPU `idm_refill_data`,
`idm_refill_lock`, RX rings, allocation/physical-address helpers, and the IDM
ops table.
Concurrency/ownership: New values stage per CPU without a local lock; reuse and
flush serialize ring commits. Pool input is unchecked.
Evidence: 90-instruction disassembly, reuse/flush/allocation helper analysis,
and ops-table entry.
Open questions: Pool/status semantics, external input contract, and doorbell
integration.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x14144-idm_rx_refill0.md`.

### 0x0e1ec cpu_register_netinfo
Status: complete
Confidence: verified copy offsets and return behavior; destination labels are
analyst names.
Role: Publish IDM net-info words and its ops pointer to CPU-net global state.
Inputs/outputs: Copies a 24-byte record; returns its 32-bit word at offset 0xc.
Globals/MMIO/callbacks: Writes one ops pointer and four noncontiguous CPU-net
globals; no MMIO.
Concurrency/ownership: No validation, synchronization, or ownership transfer.
Evidence: 13-instruction disassembly and direct idm-init caller.
Open questions: Original record/ops types and external consumers.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x0e1ec-cpu_register_netinfo.md`.

### 0x0e220 cpu_net_init
Status: complete
Confidence: verified flow/names/failure behavior; strong inference for opaque
NAPI and timer types.
Role: Obtain CPU-facing IDM TX queue records, then construct netdevs, NAPI contexts, timers, and
recycle state after IDM setup.
Inputs/outputs: Takes no semantic arguments; returns 0 on full success or -1 on
missing checked TX queue/netdev registration failure.
Globals/MMIO/callbacks: Uses `cpu_net_ops + 0x20`, CPU netdev slots, four NAPI
contexts, three timers, recycle state, and TX locks; no direct MMIO.
Concurrency/ownership: Does not unwind queues/netdevs after a later failure;
all timer/feature/recycle return values are ignored.
Evidence: 186-instruction disassembly, three callee reconstructions, direct
idm-init caller, dmesg, and runtime interfaces.
Open questions: Ops/table ABI, object ownership, NAPI layout, and teardown.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0e220-cpu_net_init.md`.

### 0x0b1d0 cpu_net_register
Status: complete
Confidence: verified allocation/offset/control behavior; field meanings partly
inferred.
Role: Allocate, initialize, register, and return one CPU-net netdev.
Inputs/outputs: Type and name input; returns null on allocation/negative
registration failure, otherwise the netdev pointer.
Globals/MMIO/callbacks: Selects ordinary or IDM netdev ops, copies default MAC,
and stores the low type byte; no MMIO.
Concurrency/ownership: Frees only the newly allocated device after local
registration failure; caller owns successful-device rollback.
Evidence: 53-instruction disassembly, four cpu-net-init callers, imports.
Open questions: Vendor netdev layout and allocation ABI.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b1d0-cpu_net_register.md`.

### 0x0b8d0 cpu_net_open
Status: complete
Confidence: verified flow/ops slots/raw state location; NAPI labels partly
inferred.
Role: Mark a CPU-net device up, turn carrier on, and enable name-selected NAPI
contexts plus IDM interrupt sources.
Inputs/outputs: Netdev input; always returns 0.
Globals/MMIO/callbacks: Uses netdev nested state, CPU-net ops slots 0x0/0x8,
four NAPI contexts, and CPU net-info source words; no direct MMIO.
Concurrency/ownership: Uses an atomic LDXR/STXR state-bit update; ignores all
called-operation statuses.
Evidence: 54-instruction disassembly, paired stop function, ops-table xrefs.
Open questions: Nested state field, NAPI ABI, source-word hardware mapping.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b8d0-cpu_net_open.md`.

### 0x0b9b0 cpu_net_stop
Status: complete
Confidence: verified flow/ops slot/raw state location; NAPI labels partly
inferred.
Role: Mark a CPU-net device down, turn carrier off, and disable name-selected
NAPI contexts plus IDM interrupt sources.
Inputs/outputs: Netdev input; always returns 0.
Globals/MMIO/callbacks: Uses netdev nested state, CPU-net ops slot 0, four NAPI
contexts, and CPU net-info source words; no direct MMIO.
Concurrency/ownership: Uses an atomic LDXR/STXR state-bit update; performs no
object teardown.
Evidence: 53-instruction disassembly, paired open function, ops-table xrefs.
Open questions: Nested state field and source-mask re-enable lifecycle.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b9b0-cpu_net_stop.md`.

### 0x0b2a8 cpu_net_timeout
Status: complete
Confidence: verified direct flow, queue/timestamp offsets, imported wake call,
and both ops-table references.
Role: Wake the selected transmit queue and refresh its timestamp after a
device-level TX timeout.
Inputs/outputs: Netdev input; semantic return type is void.
Globals/MMIO/callbacks: Reads `jiffies`; invokes `netif_tx_wake_queue`; no
direct MMIO or module-global mutation.
Concurrency/ownership: Operates on the netdev-owned queue at `device + 0x3c0`;
contains no explicit lock or allocation.
Evidence: 17-instruction disassembly, imports, and table entries at `0x1dcf8`
and `0x1df18`.
Open questions: Exact queue member name at `+0x88` is inferred as a TX
timestamp from context.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b2a8-cpu_net_timeout.md`.

### 0x0e188 cpu_net_int
Status: complete
Confidence: verified flow, context stride, accounting offsets, NAPI imports,
and all callers.
Role: Convert a masked IDM hard IRQ source to NAPI scheduling for its matching
per-source context.
Inputs/outputs: Unsigned source index input; semantic return type is void.
Globals/MMIO/callbacks: Uses the four-slot `int_info` block, then calls
`napi_schedule_prep` and conditionally `__napi_schedule`; no direct MMIO.
Concurrency/ownership: Accounting increments are non-atomic; NAPI state
arbitration is delegated to `napi_schedule_prep`; source must be in 0..3.
Evidence: 24-instruction disassembly, imported calls, all four IDM IRQ callers,
and contiguous global layout.
Open questions: Exact counter names/consumers and poll-side unmask timing.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0e188-cpu_net_int.md`.

### 0x0cce4 cpu_net_poll
Status: complete
Confidence: verified loop/control flow, count packing, ops slots, mask bits,
NAPI completion condition, and return behavior.
Role: Poll CPU RX queues for source zero, hand bounded normal/jumbo work to
`cpu_net_rx`, flush GRO, and conditionally re-enable the source.
Inputs/outputs: NAPI/budget inputs; ignores passed NAPI object; returns processed
work only when it completes NAPI, otherwise returns the original budget.
Globals/MMIO/callbacks: Uses `int_info`, raw poll counter `0x27c44`,
`cpu_net_info_word_0`, `cpu_net_ops`, `cpu_net_rx`, and GRO/NAPI helpers.
Concurrency/ownership: Uses no local lock; expects hard-IRQ source masking;
hardware count snapshot and poll counter increment are non-atomic.
Evidence: 106-instruction disassembly, NAPI registration, ops-table entries,
global xrefs, and direct RX call argument setup.
Open questions: Queue-7 persistence and exact class-enable/counter meanings.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0cce4-cpu_net_poll.md`.

### 0x0cb20 cpu_idm_poll
Status: complete
Confidence: verified loop/control flow, slot/source mapping, count packing,
class masks, completion behavior, and return paths.
Role: Poll source-2 CPU-IDM RX work and conditionally complete NAPI/re-enable
source word 8.
Inputs/outputs: NAPI/budget inputs; ignores passed NAPI object; returns processed
work after completion or original budget when exhausted.
Globals/MMIO/callbacks: Uses NAPI slot 2, raw counter `0x27f84`,
`cpu_net_info_word_8`, `cpu_net_ops`, and `cpu_net_rx`; no GRO flush.
Concurrency/ownership: Expects hard-IRQ source masking; count snapshot and
counter update have no local lock/atomic retry.
Evidence: 113-instruction disassembly, NAPI registration, net-info copy,
source-2 hard IRQ caller, and direct RX call argument setup.
Open questions: Intent behind enabled-only scanning and no-GRO policy.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0cb20-cpu_idm_poll.md`.

### 0x0c294 idm_net_poll
Status: complete
Confidence: verified fixed queue, loop/count behavior, RX order, completion
hook, NAPI slot/source mapping, and return paths.
Role: Poll source-1 IDM RX queue 8, dispatch normal/jumbo work to `idm_net_rx`,
run the optional completion hook, then conditionally re-enable source word 4.
Inputs/outputs: NAPI/budget inputs; ignores passed NAPI object; returns processed
work after completion or original budget when exhausted.
Globals/MMIO/callbacks: Uses NAPI slot 1, raw counter `0x27de4`, source word 4,
`idm_recv_cmpl`, `cpu_net_ops`, and `idm_net_rx`.
Concurrency/ownership: Expects hard-IRQ masking; queue count/counter update and
completion-hook call have no local synchronization.
Evidence: 78-instruction disassembly, NAPI registration, net-info copy,
source-1 IRQ caller, and direct IDM-RX reconstruction.
Open questions: Completion-hook lifecycle and queue-8 ownership.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0c294-idm_net_poll.md`.

### 0x0b86c cpu_rls_poll
Status: complete
Confidence: verified lock instructions, reorder callback, NAPI slot/source,
counter, and constant return.
Role: Serialize reorder-release processing, complete source-3 NAPI, and
re-enable source word c.
Inputs/outputs: Ignores NAPI/budget inputs; always returns 1.
Globals/MMIO/callbacks: Uses `idm_lock_tx`, raw counter `0x28124`, slot 3,
source word c, `net_check_reorder_rls_nolock`, and `cpu_net_ops`; no MMIO.
Concurrency/ownership: Acquires lock with LDAXR/STXR helper and releases its low
byte with STLRB; lock covers the reorder callback.
Evidence: 24-instruction body, lock-helper disassembly, reorder callback,
NAPI registration, and source-3 hard IRQ caller.
Open questions: Reorder-ring ownership and raw counter consumer.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b86c-cpu_rls_poll.md`.

### 0x0c5dc cpu_net_rx
Status: complete
Confidence: verified descriptor-loop control flow, raw fields, ops calls,
buffer lifecycle, delivery branches, and final update; field names partly
inferred.
Role: Consume CPU RX descriptors, refill/free/attach their buffers, dispatch
management/switch/stack delivery, and publish queue consumption.
Inputs/outputs: Count, queue, jumbo-selector input; always returns input count.
Globals/MMIO/callbacks: Uses CPU net ops slots 0x18/0x30/0x38/0x40/0x48, netdevs,
stats, descriptor counters, GRO/testftp/switch callbacks, and buffer globals.
Concurrency/ownership: Transfers/refills/frees raw descriptor buffers by branch;
successful GRO deliberately leaves descriptor word 0 untouched in this caller.
Evidence: 335-instruction disassembly, four poll callers, direct helper
reconstructions, and ops-table mapping.
Open questions: Descriptor/SKB field names and GRO-owned descriptor lifecycle.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0c5dc-cpu_net_rx.md`.

### 0x0bf6c idm_net_rx
Status: complete
Confidence: verified queue calculation, descriptor paths, buffer calls,
delivery branches, clears, final update, and return behavior.
Role: Consume IDM RX queues 16/17, drop jumbo descriptors, and deliver normal
descriptors through Wi-Fi trap or Linux stack paths.
Inputs/outputs: Count and selector input; always returns input count.
Globals/MMIO/callbacks: Uses IDM netdev, jumbo-error counter, skb/Wi-Fi callback,
ops slots 0x18/0x30/0x38/0x40/0x48, and buffer globals.
Concurrency/ownership: Clears descriptor word 0 on drop/failure but not after
successful delivery; final update owns subsequent ring progression.
Evidence: 200-instruction disassembly, two poll callers, helper decompilation,
and ops-table mapping.
Open questions: Jumbo-drop rationale, skb ownership, success-path descriptor
lifecycle.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0bf6c-idm_net_rx.md`.

### 0x0c3ec cpu_sw_rx
Status: complete
Confidence: verified predicate, callback branches, netdev overwrite, metadata
arguments, counter, and return.
Role: Route qualifying CPU RX descriptors to IDM trap delivery, otherwise to
the switch skb callback.
Inputs/outputs: Six arguments; only skb, descriptor, and queue are used;
always returns 0.
Globals/MMIO/callbacks: Uses raw counter `0x28140`, IDM netdev,
`idm_skb_recv`, `switch_skb_recv`, and `idm_set_wifi_trap_info`; no MMIO.
Concurrency/ownership: Assumes switch callback is non-null on fallback; caller
performs that check.
Evidence: 49-instruction body, sole CPU-RX caller, and trap-info helper.
Open questions: Predicate semantics and callback ownership.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0c3ec-cpu_sw_rx.md`.

### 0x0c4b0 cpu_omci_rx
Status: complete
Confidence: verified callbacks, null behavior, work-mode branch, MIC check,
metadata port handling, and returns.
Role: Deliver a CPU management descriptor to the OMCI/OAM callback with
mode-specific port and MIC validation behavior.
Inputs/outputs: Descriptor, metadata, data, and length; returns -1 only after a
nonzero configured MIC check, otherwise 0.
Globals/MMIO/callbacks: Uses work mode, OMCI/OAM callback, MIC callback, and
local OMCI port ID; no direct MMIO.
Concurrency/ownership: No locking, allocation, or buffer ownership operation.
Evidence: 74-instruction body, sole CPU-RX caller, and matching init work-mode
mask.
Open questions: Callback registration and MIC contract.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0c4b0-cpu_omci_rx.md`.

### 0x0d668 cpu_net_tx
Status: complete
Confidence: verified dispatch, policy/lock/error behavior, queue ownership,
backend calls, drops, return value, and raw offsets; descriptor labels inferred.
Role: Dispatch SW, PON, and OMCI/OAM Linux TX to IDM hardware queues.
Inputs/outputs: skb/netdev input; always returns `NETDEV_TX_OK` after consuming
the skb or retaining it in completion ownership.
Globals/MMIO/callbacks: Uses readiness hook, netdev type, two TX locks/queues,
PON policy, MIC/QoS/GSO callbacks, ops slots 0x68/0x78, and TX stats.
Concurrency/ownership: Locks per interface; success transfers skb to queue
owner storage, failures free it; no Linux TX backpressure is exposed.
Evidence: `0x710` ARM64 function, netdev ops table, queue/reclaim/backends,
timer and GSO paths.
Open questions: Descriptor/QoS/FFE fields, nonlinear PON padding contract, and
tagged GSO owner format.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0d668-cpu_net_tx.md`.

### 0x0dd78 idm_tx_test
Status: complete. Exported fixed-packet test sender with raw six-register ABI,
port-dependent netdev selection, and repeated skb cloning. Record:
`functions/0x0dd78-idm_tx_test.md`.

### 0x0d234 idm_net_tx
Status: complete
Confidence: verified GSO gate, locks, descriptor/backend/owner paths, stats,
and return behavior.
Role: Transmit IDM netdev skbs through GSO or the Wi-Fi IDM TX backend.
Inputs/outputs: skb/netdev input; returns -1 only for descriptor exhaustion,
otherwise 0.
Globals/MMIO/callbacks: Uses GSO enable, two TX locks, IDM queue, ops slot 0x80,
Wi-Fi backend, stats, and queue-owner state.
Concurrency/ownership: GSO consumes original skb; direct success retains it for
completion; direct failures free and roll back as applicable.
Evidence: 153-instruction body, type-3 ops-table entry, Wi-Fi backend, and
shared queue/reclaim functions.
Open questions: Nonzero descriptor-exhaustion contract, skb bit 14, and control
word semantics.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0d234-idm_net_tx.md`.

### 0x0d5ac cpu_net_pon_set_desc
Status: complete
Confidence: verified descriptor/QoS/metadata paths and sole caller; semantic
return type void.
Role: Configure PON TX descriptor control/QoS bits and skb port metadata.
Inputs/outputs: skb and descriptor input; caller ignores residual return value.
Globals/MMIO/callbacks: Uses work mode, PON/LAN state, QoS callback, descriptor
offsets, and skb byte 0x108; no MMIO.
Concurrency/ownership: No lock, allocation, or ownership transfer.
Evidence: 47-instruction body, sole CPU-TX caller, and callback register setup.
Open questions: Descriptor control/QoS semantics and skb metadata meaning.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0d5ac-cpu_net_pon_set_desc.md`.

### 0x14a30 idm_cpu_tx
Status: complete
Confidence: verified descriptor/padding/port/barrier/doorbell flow and return.
Role: Program and submit a direct SW/PON IDM TX descriptor.
Inputs/outputs: skb and descriptor input; always returns 0.
Globals/MMIO/callbacks: Uses data-padding, descriptor offsets, skb metadata,
first TX doorbell, and DSB ST; no lock.
Concurrency/ownership: Does not free or retain skb; caller owns queue tracking.
Evidence: 86-instruction body, IDM ops-table entry, and CPU-TX caller context.
Open questions: Descriptor port codes and post-address length mutation contract.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x14a30-idm_cpu_tx.md`.

### 0x1493c idm_omci_tx
Status: complete
Confidence: verified descriptor/minimum-length/barrier/doorbell flow and return.
Role: Program and submit direct OMCI/OAM IDM TX descriptors.
Inputs/outputs: skb and descriptor input; always returns 0.
Globals/MMIO/callbacks: Uses descriptor/skb fields, management TX doorbell, and
DSB ST; no lock or callback.
Concurrency/ownership: Does not free/retain skb; caller owns queue tracking.
Evidence: 61-instruction body, IDM ops-table entry, and CPU-TX caller context.
Open questions: Descriptor bit/class/doorbell meanings and short-length safety.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x1493c-idm_omci_tx.md`.

### 0x14be4 idm_wifi_tx
Status: complete
Confidence: verified descriptor/padding/port/barrier/doorbell flow and return.
Role: Program and submit direct IDM/Wi-Fi TX descriptors.
Inputs/outputs: skb and descriptor input; always returns 0.
Globals/MMIO/callbacks: Uses data padding, descriptor/skb fields, third TX
doorbell, and DSB ST; no lock.
Concurrency/ownership: Does not free/retain skb; caller owns queue tracking.
Evidence: 61-instruction body, IDM ops-table slot 0x80, and IDM-TX caller.
Open questions: Descriptor meanings, doorbell distinction, padding contract.
Recovered source: `recovered/plat_idm.c`; detailed record:
`functions/0x14be4-idm_wifi_tx.md`.

### 0x0b4fc net_check_tx_done_nolock
Status: complete
Confidence: verified completion/wrap/owner/queue/global flow; return register
behavior is documented as residual.
Role: Reclaim completed TX owner slots and advance TX queue completion state.
Inputs/outputs: TX queue input; return is hardware done when unchanged or old
pending count when changed; all in-module callers ignore it.
Globals/MMIO/callbacks: Uses ops slot 0x50, owner-free helpers, IDM queue,
completion/error globals, and queue fields; no direct MMIO.
Concurrency/ownership: Explicitly nolock; caller serializes; tagged owners,
Wi-Fi stack push, and ordinary skbs have different reclamation paths.
Evidence: 128-instruction body, timer/allocation callers, and ops-table mapping.
Open questions: Tagged owner convention, Wi-Fi ownership, completion width.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0b4fc-net_check_tx_done_nolock.md`.

### 0x0b7a4 cpu_timer_func
Status: complete
Confidence: verified threshold gate, lock/reclaim order, reorder call, global
timer rearm, ignored argument, and registration.
Role: Periodically reclaim TX completions, process reorder release, and rearm
the global CPU-net timer.
Inputs/outputs: Ignores timer callback argument; semantic return type void.
Globals/MMIO/callbacks: Uses threshold, three TX locks/queues, reorder helper,
`jiffies`, global timer, and `add_timer_on`; no direct MMIO.
Concurrency/ownership: Reclaims under matching raw locks; reorder shares
`idm_lock_tx` with NAPI release processing.
Evidence: 50-instruction body, init registration, unlock-timer helper, and
shared reclaim function.
Open questions: Threshold semantics/width and unlock-timer context.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0b7a4-cpu_timer_func.md`.

### 0x0f87c net_gso_tx
Status: complete
Confidence: verified gates, callback selection, stats/counters, skb device
assignment, return behavior, and callers.
Role: Route an skb to upload or TCP GSO processing and account local outcomes.
Inputs/outputs: skb/device/path input; always returns 0 and does not free the
original skb.
Globals/MMIO/callbacks: Uses upload mode, GSO counters, three GSO callbacks,
raw skb fields, and device stats; no direct MMIO.
Concurrency/ownership: Caller holds TX lock and frees original skb after return;
child GSO work owns generated nbufs.
Evidence: 79-instruction body and all three TX caller paths.
Open questions: Upload flag/policy and child GSO ownership.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0f87c-net_gso_tx.md`.

### 0x10eac pp_net_tcp_gro
Status: complete
Confidence: verified for control flow, flow state, eligibility/flush/ownership
behavior, return values, and caller; strong inference for raw descriptor/SKB
field labels and fragment encoding.
Role: Aggregate qualifying CPU RX IPv4/TCP traffic into up to 16 pending flow
skbs, then flush the aggregate on a short final payload or fragment limit.
Inputs/outputs: Raw descriptor, netdev, mapped packet data, queue, and jumbo
selector; returns 1 only after taking packet-buffer ownership into GRO state,
otherwise 0 for ordinary CPU RX handling.
Globals/MMIO/callbacks: Uses a 16-bucket GRO hlist, flow count, port allow
lists, Wi-Fi descriptor snapshot, GRO/debug counters, skb raw fields, and
switch/IDM delivery indirectly through `pp_tcp_gro_flush`; no direct MMIO.
Concurrency/ownership: No local synchronization; new/continued packet buffers
transfer to aggregate skb ownership. Eligibility failure flushes and frees every
pending flow. The binary's hash-only append gate lacks a collision null guard.
Evidence: Complete 406-instruction disassembly, sole CPU-RX caller, and direct
helper reconstructions for eligibility, lookup, flush, and flush-all.
Open questions: Vendor descriptor/SKB field names, fragment-page encoding,
Wi-Fi snapshot concurrency, and remaining `can_tcp_gro` field semantics.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10eac-pp_net_tcp_gro.md`.

### 0x10a9c can_tcp_gro
Status: complete
Confidence: verified predicates, arithmetic, return values, and sole caller;
field/region labels are strong inference.
Role: Decide whether a candidate packet may start or continue a TCP GRO flow.
Inputs/outputs: Existing aggregate skb or null, IPv4/TCP pointers, descriptor;
returns 1 only after all raw address/flag/length/tuple/sequence predicates pass.
Globals/MMIO/callbacks: Reads reserved-pool configuration, flow count, descriptor
byte 6, and raw skb fields; no MMIO, callbacks, writes, locks, or allocation.
Concurrency/ownership: No ownership transfer. With `g_cur_flows > 15` and a
null flow pointer, it falls through to an unguarded existing-flow dereference.
Evidence: Complete 98-instruction disassembly and direct argument setup from
`pp_net_tcp_gro`.
Open questions: Reserved-region role, TCP flag mask semantics, and skb `+0xac`.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10a9c-can_tcp_gro.md`.

### 0x10930 is_l4port_supported
Status: complete
Confidence: verified list selection/traversal, lookup result, BH lock/release,
and both callers; list/lock labels are strong inference.
Role: Test a configured TCP source or destination port against the selected
GRO allow list.
Inputs/outputs: 16-bit port and low-byte destination selector; returns 1 on
matching list entry, otherwise 0.
Globals/MMIO/callbacks: Reads source/destination sentinel lists and releases
`groport_busy_lock`; no MMIO, allocation, callback, or list mutation.
Concurrency/ownership: Walks under a specialized BH-disabling lock, then uses a
byte-width store-release and `__local_bh_enable_ip` on every result path.
Evidence: Complete 40-instruction disassembly and two direct callers in
`pp_net_tcp_gro`.
Open questions: Original lock/entry types and port-list lifetime/registration.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10930-is_l4port_supported.md`.

### 0x10e5c search_gro_flow
Status: complete
Confidence: verified bucket arithmetic, hlist traversal, hash-only comparison,
return behavior, and sole caller.
Role: Determine whether any GRO flow in the selected bucket has a matching hash.
Inputs/outputs: 32-bit flow hash; returns 1 for a matching flow hash, 0 otherwise.
Globals/MMIO/callbacks: Reads the 16-bucket GRO hlist and flow hash at `+0x8`;
no MMIO, write, lock, allocation, or callback.
Concurrency/ownership: No synchronization or ownership transfer. It does not
test the exact tuple, so parent code owns collision safety.
Evidence: Complete 20-instruction disassembly and sole caller at `0x11168` in
`pp_net_tcp_gro`.
Open questions: Original flow allocation type name only.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10e5c-search_gro_flow.md`.

### 0x10c24 pp_tcp_gro_flush
Status: complete
Confidence: verified raw skb/header mutation, delivery branch, callback
arguments, counters, return use, and callers; field labels are strong inference.
Role: Finalize IPv4 length/checksum for a pending GRO aggregate and hand its skb
to Wi-Fi trap or switch delivery.
Inputs/outputs: Aggregate skb input; semantic return type void.
Globals/MMIO/callbacks: Uses shared-info, Wi-Fi descriptor/queue/netdev,
receiver callbacks, checksum helper, GRO counters, and SMB state; no MMIO.
Concurrency/ownership: No local lock; callback takes aggregate skb ownership.
It does not free the skb or its enclosing GRO flow allocation.
Evidence: Complete 106-instruction disassembly and three direct callers.
Open questions: Raw skb field names, callback lifetime, and accounting meanings.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10c24-pp_tcp_gro_flush.md`.

### 0x10dcc pp_tcp_gro_flush_all
Status: complete
Confidence: verified complete sweep/control flow, cleanup order, return use,
and both callers.
Role: Flush, unlink, count down, and free every pending flow in all 16 GRO buckets.
Inputs/outputs: No semantic input; semantic return type void.
Globals/MMIO/callbacks: Uses GRO hash table/count, flow allocations, hlist
unlink, `pp_tcp_gro_flush`, and `kfree`; no direct MMIO.
Concurrency/ownership: No local lock; flush transfers each aggregate skb to its
receiver, then this function frees the enclosing flow allocation.
Evidence: Complete body, two direct caller xrefs, and parent inlined equivalent.
Open questions: Existing GRO table synchronization and receiver ownership only.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10dcc-pp_tcp_gro_flush_all.md`.

### 0x1150c net_gro_init
Status: complete
Confidence: verified.
Role: Publish the default SMB-test configuration callback for the GRO subsystem.
Inputs/outputs: No semantic input or return.
Globals/MMIO/callbacks: Writes `pp_smb_test_config` with
`lower_net_smb_test_config`; no MMIO or other global mutation.
Concurrency/ownership: No visible synchronization, allocation, or ownership
transfer; initialization caller establishes callback lifetime.
Evidence: Complete four-instruction body and sole `cpu_net_init` caller.
Open questions: Callback invocation/registration and exact callback contract.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x1150c-net_gro_init.md`.

### 0x10778 lower_net_smb_test_config
Status: complete
Confidence: verified branch/global/return behavior, affinity-mask arithmetic,
and callback publication; bitmap type labels are strong inference.
Role: Enable or disable GRO/SMB test state through the callback installed by
`net_gro_init`.
Inputs/outputs: Nonzero enables and returns 1; zero disables, resets a CPU-IDM
IRQ affinity hint, and returns 2. Both store the result to a 16-bit threshold.
Globals/MMIO/callbacks: Writes GRO/SMB/threshold globals; reads CPU/IRQ globals
and calls `irq_set_affinity_hint`; no direct MMIO.
Concurrency/ownership: No local lock, allocation, or ownership transfer.
Evidence: Complete 33-instruction body and sole callback publication xref.
Open questions: External callback invoker/value semantics and CPU bitmap ABI.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10778-lower_net_smb_test_config.md`.

### 0x10750 __fswab32_0
Status: complete
Confidence: verified.
Role: Reverse byte order of one 32-bit value.
Inputs/outputs: One 32-bit value; returns its byte reversal.
Globals/MMIO/callbacks: None.
Concurrency/ownership: No state or ownership behavior.
Evidence: Complete two-instruction body and six direct callers.
Open questions: None.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10750-fswab32_0.md`.

### 0x10758 hlist_del_init
Status: complete. GRO hlist unlink-and-clear helper with a null-`pprev` no-op.
Record: `functions/0x10758-hlist_del_init.md`.

### 0x10738 sub_10738
Status: complete
Confidence: verified raw instruction sequence and zero direct xrefs; purpose
unknown.
Role: Read/temporarily mutate/restore `ICC_PMR_EL1`, issue `DSB SY`, then return
the byte-swapped low 32 bits of `TPIDR_EL2` via fall-through to `__fswab32_0`.
Inputs/outputs: No input; returns byte-swapped low `TPIDR_EL2` word.
Globals/MMIO/callbacks: Uses CPU system registers only; no module globals,
allocation, callback, or conventional MMIO.
Concurrency/ownership: Touches CPU-local interrupt-controller state; no lock or
ownership transfer.
Evidence: Complete six-instruction body/fall-through and no direct IDB xrefs.
Open questions: PMR toggle rationale and external/indirect reachability.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10738-sub_10738.md`.

### 0x10808 __raw_spin_lock_bh.constprop.13
Status: complete
Confidence: verified CPU-local increment, lock fast/slow paths, caller set, and
return use; context-field label is inferred.
Role: Enter the BH-disabled lock region protecting configured GRO port lists.
Inputs/outputs: No semantic input or return.
Globals/MMIO/callbacks: Reads/writes `SP_EL0 + 0x10`, acquires 32-bit
`groport_busy_lock`, and may call `queued_spin_lock_slowpath`; no direct MMIO.
Concurrency/ownership: Acquire exclusive fast path; caller releases low lock
byte and restores BH state. No allocation or ownership transfer.
Evidence: Complete 23-instruction body, three callers, and caller release code.
Open questions: Vendor context/lock ABI and byte-width unlock rationale.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10808-raw_spin_lock_bh_constprop_13.md`.

### 0x10864 add_supported_l4port
Status: complete
Confidence: verified allocation/init/insertion/lock/return behavior and zero
direct xrefs; cache/list labels are strong inference.
Role: Allocate and append one TCP port to the source or destination GRO allow list.
Inputs/outputs: 16-bit port and low-byte destination selector; returns 1 on
insertion or -1 after allocation failure.
Globals/MMIO/callbacks: Uses shared GRO state cache, source/destination circular
lists, port lock, BH enable helper, and failure log; no direct MMIO.
Concurrency/ownership: Locks list mutation; successful entry remains list-owned.
No deduplication or rollback occurs.
Evidence: Complete 48-instruction body, raw list writes, zero direct IDB xrefs.
Open questions: Cache/string identity and external registration/lifetime policy.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x10864-add_supported_l4port.md`.

### 0x109d0 remove_supported_l4port
Status: complete
Confidence: verified selection/traversal/unlink/poison/free/release/return
behavior and zero direct xrefs.
Role: Remove the first matching TCP port from a selected GRO allow list.
Inputs/outputs: 16-bit port and low-byte destination selector; always returns 1.
Globals/MMIO/callbacks: Uses source/destination lists, port lock, BH enable, and
`kfree`; no direct MMIO or callback.
Concurrency/ownership: List mutation is locked; matched list-owned entry is
poisoned then freed. Missing entries and duplicates beyond first match are ignored.
Evidence: Complete 49-instruction body, raw unlink/poison/free sequence, and
zero direct IDB xrefs.
Open questions: External config API, lifetime, and duplicate policy.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x109d0-remove_supported_l4port.md`.

### 0x0f9bc net_gso_init
Status: complete
Confidence: verified proc/global/callback branches, caller, and return use;
proc-operations ABI is inferred.
Role: Enable upload driver state, create upload control proc entries, and publish
the upload hook.
Inputs/outputs: No semantic input or return.
Globals/MMIO/callbacks: Writes `g_upload_driver_en` and `upload_hook`; creates
proc entries, ratelimited logs errors, and uses `upload_test_fops`; no MMIO.
Concurrency/ownership: No lock or rollback. Callback publication occurs even on
proc failures; local proc objects are not cleaned up here.
Evidence: Complete 35-instruction body and sole CPU-net-init caller.
Open questions: Proc ABI/teardown and upload-hook invocation contract.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0f9bc-net_gso_init.md`.

### 0x0ea40 net_upload_fun
Status: complete
Confidence: verified lock/reference-count/transition behavior, direct caller,
and hook publication; lock-context labels are strong inference.
Role: Serialize and reference-count upload GSO mode enable/disable transitions.
Inputs/outputs: Nonzero acquires an upload reference; zero releases one when
present. Semantic return type void.
Globals/MMIO/callbacks: Uses debug/count/lock globals and upload enable/disable
helpers; no direct MMIO, allocation, or skb ownership.
Concurrency/ownership: Acquires `net_lock_tx` with BH-disabled exclusive lock;
zero-disable still calls disable helper when count remains zero.
Evidence: Complete 68-instruction body, direct proc caller, and hook publication.
Open questions: Slowpath-context ABI and enable/disable helper internals.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0ea40-net_upload_fun.md`.

### 0x0eb50 upload_write_proc
Status: complete
Confidence: verified user-range/copy/parse/callback/return behavior and proc
operations xref; range-check label is inferred.
Role: Parse upload control proc writes and call `net_upload_fun` with an 8-bit
decimal value.
Inputs/outputs: File/user buffer/count; returns -1 on validation/copy failure,
otherwise original count.
Globals/MMIO/callbacks: Uses inlined user access validation, copy/conversion,
and `net_upload_fun`; no direct MMIO or global mutation.
Concurrency/ownership: No local lock or allocation. Ignores count for the fixed
8-byte read and does not ensure NUL termination before conversion.
Evidence: Complete 59-instruction body, proc-ops data xref, direct hook call.
Open questions: User-range macro/ABI and external write policy.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0eb50-upload_write_proc.md`.

### 0x0e964 gso_upload_enable
Status: complete
Confidence: verified gate/allocation/init/failure cleanup/global publication and
sole caller; nbuf field labels are strong inference.
Role: Lazily allocate and initialize the fixed 64-entry upload GSO nbuf pool.
Inputs/outputs: No semantic input or return.
Globals/MMIO/callbacks: Uses GSO pool/count/index/length globals, nbuf allocator,
ratelimited logger, and paired disable cleanup; no MMIO.
Concurrency/ownership: No local lock; caller holds TX lock. Success transfers
64 nbufs to pool ownership; partial allocation leaks because count remains zero
when the cleanup helper is called.
Evidence: Complete 53-instruction body and sole net-upload caller.
Open questions: Nbuf ABI/field names and disable cleanup behavior.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0e964-gso_upload_enable.md`.

### 0x0e89c gso_upload_disable
Status: complete
Confidence: verified mode-dependent loop/free/reset behavior and both callers;
nbuf field labels are inferred.
Role: Reset or release currently published upload GSO pool buffers.
Inputs/outputs: Nonzero frees visible pool buffers and clears count; zero resets
visible buffer data only. Semantic return type void.
Globals/MMIO/callbacks: Uses pool/count/last-header globals, nbuf free helper,
and buffer-size globals; no MMIO.
Concurrency/ownership: No local lock. It re-reads count each iteration, skips
null slots, and does not reset index. Partial-enable failure leaks because count
is zero before this helper can see stored slots.
Evidence: Complete 50-instruction body and both direct callers.
Open questions: Nbuf ABI and any external leak recovery path.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0e89c-gso_upload_disable.md`.

### 0x0afd8 cpu_net_alloc_nbuf
Status: complete
Confidence: verified.
Role: Return an nbuf from IDM ops-table callback `+0x28`.
Inputs/outputs: No input; returns callback pointer unchanged.
Globals/MMIO/callbacks: Reads `cpu_net_ops` and calls offset `+0x28`; no MMIO.
Concurrency/ownership: No validation/lock/ownership behavior in wrapper.
Evidence: Complete eight-instruction body, four callers, and ops-table mapping.
Open questions: Allocated nbuf type and callback ownership contract.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0afd8-cpu_net_alloc_nbuf.md`.

### 0x0b4b0 cpu_net_free_nbuf
Status: complete
Confidence: verified gate/pointer arithmetic/ops callback/counter and five
callers; nbuf field labels are strong inference.
Role: Conditionally release an nbuf through IDM ops callback `+0x30`.
Inputs/outputs: Nbuf input; semantic return type void.
Globals/MMIO/callbacks: Reads nbuf fields, calls `cpu_net_ops + 0x30`, or writes
the non-release counter; no MMIO.
Concurrency/ownership: Bit 1 at nbuf `+0x2c` gates release. Set means retain
and account; clear transfers buffer to backend free callback.
Evidence: Complete 19-instruction body, five callers, and ops-table mapping.
Open questions: Nbuf ABI/field names and non-release counter semantics.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b4b0-cpu_net_free_nbuf.md`.

### 0x0e634 net_gso_upload_send
Status: complete
Confidence: verified descriptor failure/setup/GSO encoding/barrier/handoff and
both callers; descriptor field labels are strong inference.
Role: Program and submit one upload-GSO nbuf descriptor.
Inputs/outputs: Nbuf, skb, payload length, and segment size; returns -1 for no
TX descriptor, otherwise returns downstream descriptor-submit status.
Globals/MMIO/callbacks: Uses CPU TX queue, nbuf free/config/physical helpers,
descriptor fields, submission counters, and descriptor handoff; no MMIO.
Concurrency/ownership: No local lock. Failure invokes gated nbuf free; success
hands nbuf/descriptor onward after `DSB ST`.
Evidence: Complete 85-instruction body, both segmenter callers, raw setup path.
Open questions: Descriptor ABI/bit names and submission ownership.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0e634-net_gso_upload_send.md`.

### 0x0ec3c net_tcp_gso_tx_upload
Status: complete
Confidence: verified header/pool/checksum/submit/failure flow and sole caller;
raw field/marker labels are strong inference.
Role: Build one upload-GSO nbuf template from an skb and submit it.
Inputs/outputs: Skb, unused netdev, and path bit; returns downstream submit
status or -1 when no nbuf is available.
Globals/MMIO/callbacks: Uses GSO pool/index/header state, counters, opaque
network-header helper, checksum helpers, nbuf allocator, and upload send helper;
no direct MMIO.
Concurrency/ownership: No local lock. It may dereference a null pool slot before
its null test when stale header state is larger; ownership transfers downstream
only after successful submit.
Evidence: Complete `0x2fc`-byte body, sole caller, and direct helper setup.
Open questions: Raw ABI, outer marker, payload association, pool-null invariant.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0ec3c-net_tcp_gso_tx_upload.md`.

### 0x0ef38 net_tcp_gso_tx_upload1
Status: complete
Confidence: verified segmentation/header/sequence/checksum/failure flow and
sole caller; raw field/marker labels are strong inference.
Role: Software-segment upload-mode GSO into one fresh nbuf per payload segment.
Inputs/outputs: Skb, unused netdev, and path bit; returns 0 on full submit,
otherwise -1 for allocation or downstream submit failure.
Globals/MMIO/callbacks: Uses GSO size/debug/counters, opaque network-header
helper, nbuf allocator, checksum helpers, and upload send; no direct MMIO.
Concurrency/ownership: No local lock. Each successful nbuf transfers to submit
path; negative send ownership remains delegated downstream.
Evidence: Complete 199-instruction body and sole alternate-mode caller.
Open questions: Raw ABI, outer marker, payload association, submit ownership.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0ef38-net_tcp_gso_tx_upload1.md`.

### 0x0f258 net_tcp_gso_tx
Status: complete
Confidence: verified validation, fragment/linear sourcing, segment construction,
checksum/descriptor branches, ownership, returns, caller, and helpers; raw ABI
labels are strong inference.
Role: Software-segment ordinary TCP GSO skb data into CPU TX nbuf descriptors.
Inputs/outputs: Skb, unused netdev, direction bit; returns 0, -1 on resource or
submit failure, or -3 for header beyond linear data.
Globals/MMIO/callbacks: Uses shared-info GSO/fragments, CPU TX queue/ops,
checksum policy, counters/stats/state, nbuf allocation/free, and submit helpers;
no direct MMIO.
Concurrency/ownership: Caller serializes. Successful nbuf transfers to CPU TX
owner ring; failures may leave earlier segments sent. Fragment exhaustion sends
a potentially truncated final segment yet returns success.
Evidence: Complete 42-block `0x624` body, sole caller, and child helper analysis.
Open questions: Fragment ABI, descriptor/stats fields, state policy/invariants.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0f258-net_tcp_gso_tx.md`.

### 0x0e7f4 net_gso_checksum_upload
Status: complete
Confidence: verified checksum inputs/template branch/header mutations/return use
and callers; raw header labels are strong inference.
Role: Compute TCP pseudo-header checksum and IPv4 header checksum for GSO data.
Inputs/outputs: IPv4 header, TCP header, upload-template selector; semantic
return type void.
Globals/MMIO/callbacks: Calls checksum imports only; no module globals or MMIO.
Concurrency/ownership: Mutates supplied headers only; no lock or ownership flow.
Evidence: Complete body and all three sender contexts.
Open questions: Upload-template checksum/offload contract.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0e7f4-net_gso_checksum_upload.md`.

### 0x0e788 net_gso_ipv6tcp_checksum.constprop.6
Status: complete
Confidence: verified.
Role: Compute TCP checksum for an IPv6 GSO header template.
Inputs/outputs: IPv6/TCP header pointers; semantic return type void.
Globals/MMIO/callbacks: Calls checksum imports only; no module globals or MMIO.
Concurrency/ownership: Mutates supplied TCP header only.
Evidence: Complete 27-instruction body and two upload-segmenter callers.
Open questions: None beyond raw header ABI already recorded.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0e788-net_gso_ipv6tcp_checksum.md`.

### 0x0df4c net_cfg_desc_by_skb
Status: complete
Confidence: verified writes, port branch, return use, and all callers;
descriptor labels are strong inference.
Role: Initialize common GSO TX descriptor fields from an skb/path selector.
Inputs/outputs: Descriptor, skb, direction; semantic return type void.
Globals/MMIO/callbacks: Reads `lan_up`, skb port byte, and mutates descriptor;
no MMIO, callback, allocation, or global write.
Concurrency/ownership: No lock or ownership behavior.
Evidence: Complete 23-instruction body and three direct callers.
Open questions: Descriptor constant/port mapping semantics.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0df4c-net_cfg_desc_by_skb.md`.

### 0x0dfa8 cpu_net_nb_desc_tx
Status: complete
Confidence: verified owner-slot/tag/pending/ops update order, return, and both
callers; CPU TX queue labels are strong inference.
Role: Commit a tagged nbuf owner to CPU TX queue and notify backend of one entry.
Inputs/outputs: Nbuf and descriptor; always returns 0.
Globals/MMIO/callbacks: Uses CPU TX queue and `cpu_net_ops + 0x70`; no MMIO.
Concurrency/ownership: No local lock. Successful call transfers nbuf to tagged
owner-ring completion ownership.
Evidence: Complete 23-instruction body, both GSO callers, and reclaim cross-check.
Open questions: Queue ABI and ops `+0x70` semantics.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0dfa8-cpu_net_nb_desc_tx.md`.

### 0x0ce8c net_get_next_txdesc
Status: complete
Confidence: verified threshold/reclaim/full behavior, producer wrap, fixed
descriptor stride, globals, and all direct callers.
Role: Reserve the next 32-byte TX descriptor without submitting it.
Inputs/outputs: TX queue input; returns the reserved descriptor or null when
post-reclaim pending is at least queue depth.
Globals/MMIO/callbacks: Reads low 16 bits of `g_net_check_threshold`; calls
`net_check_tx_done_nolock`; increments `net_tx_full` on fullness; no MMIO.
Concurrency/ownership: No local lock or ownership transfer. Callers must
serialize queue mutation and separately commit or roll back the reservation.
Evidence: Complete 33-instruction body, eight direct xrefs, completion and
rollback helper cross-checks.
Open questions: Threshold configuration rationale and its pending/depth invariant.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0ce8c-net_get_next_txdesc.md`.

### 0x0aff8 net_set_prev_txdesc
Status: complete
Confidence: verified full body, producer/depth fields, all callers, and failure
context.
Role: Roll back one prior TX descriptor producer reservation.
Inputs/outputs: TX queue input; semantic return type void, although machine
code leaves the input pointer in its return register.
Globals/MMIO/callbacks: Queue producer/depth fields only; no globals, MMIO, or
callbacks.
Concurrency/ownership: No local lock or ownership mutation; callers serialize
and invoke it after direct backend TX submission failure.
Evidence: Complete six-instruction body, four xrefs, and reserve-helper pairing.
Open questions: Shared queue ABI and caller locking contract.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0aff8-net_set_prev_txdesc.md`.

### 0x0cf14 cpu_net_nb_tx
Status: complete
Confidence: verified list/selector/descriptor/owner/batching/error/stats/return
behavior and export boundary; nbuf and descriptor labels are strong inference.
Role: Submit an externally supplied linked nbuf list through a selected unlock TX
queue in batches of at most 256 descriptors.
Inputs/outputs: Nbuf-list head; returns -1 only for selector > 1, otherwise 0.
Globals/MMIO/callbacks: Reads per-CPU CPU number, selector map, unlock queues,
debug controls, stats, and CPU-net ops `+0x70`; no direct MMIO.
Concurrency/ownership: No local lock/barrier. Successful nbufs become tagged
owner entries; exhausted descriptors use the nbuf-free release gate.
Evidence: Complete 198-instruction body, no internal xrefs, runtime export, and
`ipsec.ko` undefined-symbol evidence.
Open questions: Nbuf producer/template ABI, selector lifecycle, and caller lock.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0cf14-cpu_net_nb_tx.md`.

### 0x0d49c cpu_lowpower_tx
Status: complete
Confidence: verified callback/device/mode branches, repeated judge behavior,
descriptor update, return use, and both callers; callback semantics are inferred.
Role: Invoke an optional low-power TX callback and conditionally adjust direct-TX
descriptor encoded length.
Inputs/outputs: Device, skb, descriptor; semantic return type void.
Globals/MMIO/callbacks: Uses two externally registered low-power callbacks and
`g_pon_work_mode`; no direct MMIO.
Concurrency/ownership: No local lock/barrier/allocation/ownership change; caller
holds `net_lock_tx` in both direct uses.
Evidence: Complete 68-instruction body, two caller xrefs, callback registrations.
Open questions: Callback purpose, judge value 2, port 64, mode/length semantics.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0d49c-cpu_lowpower_tx.md`.

### 0x0afc0 regisetr_low_power_send_pkt_handle
Status: complete
Confidence: verified pointer store/return, export, and external import boundary.
Role: Publish or clear the low-power five-argument send callback.
Inputs/outputs: Callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes `low_power_send`; no MMIO.
Concurrency/ownership: Unsynchronized callback publication; no lifetime/refcount
handling or ownership transfer.
Evidence: Complete three-instruction body, no internal callers, runtime export,
and `np.ko` undefined-symbol evidence.
Open questions: Provider lifetime and callback argument semantics.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0afc0-regisetr_low_power_send_pkt_handle.md`.

### 0x0afcc regisetr_low_power_up_en_judge_handle
Status: complete
Confidence: verified pointer store/return, export, and external import boundary.
Role: Publish or clear the no-argument low-power judge callback.
Inputs/outputs: Callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes `low_power_up_en_judge`; no MMIO.
Concurrency/ownership: Unsynchronized callback publication with no lifetime or
reference handling.
Evidence: Complete three-instruction body, no internal callers, runtime export,
and `np.ko` undefined-symbol evidence.
Open questions: Provider lifetime and judge return-value semantics.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0afcc-regisetr_low_power_up_en_judge_handle.md`.

### 0x0af68 register_omci_oam_handle
Status: complete
Confidence: verified pointer store/return, exports, consumer, and companion
global-import boundary.
Role: Publish or clear the OMCI/OAM receive callback.
Inputs/outputs: Callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes exported `omci_oam_rx`; no MMIO.
Concurrency/ownership: Unsynchronized publication with no lifetime/refcount
handling; the slot is separately externally writable.
Evidence: Complete three-instruction body, no internal callers, runtime exports,
`np.ko` slot import, and CPU-OMCI consumer cross-check.
Open questions: Provider lifecycle and rationale for direct slot export.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0af68-register_omci_oam_handle.md`.

### 0x0af74 regisetr_omci_mic_add_handle
Status: complete
Confidence: verified pointer store/return, export, and management-TX consumer.
Role: Publish or clear the OMCI MIC-add data/length callback.
Inputs/outputs: Callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes `omci_mic_add`; no MMIO.
Concurrency/ownership: Unsynchronized publication with no lifetime/refcount
handling.
Evidence: Complete three-instruction body, no internal callers, runtime export,
and CPU-management-TX consumer xref.
Open questions: Callback provider/lifetime and MIC status contract.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0af74-regisetr_omci_mic_add_handle.md`.

### 0x0af80 idm_omci_portid_set
Status: complete
Confidence: verified exact empty body, no direct callers, and export.
Role: Exported no-op stub despite its port-ID setter name.
Inputs/outputs: No semantic input or output; all ABI argument registers ignored.
Globals/MMIO/callbacks: None; specifically does not write `local_omci_port_id`.
Concurrency/ownership: No behavior.
Evidence: One `RET` instruction, no IDA callers, runtime export, and slot xrefs.
Open questions: Rationale for the intentionally inert exported ABI.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0af80-idm_omci_portid_set.md`.

### 0x0af84 register_omci_mic_check_handle
Status: complete
Confidence: verified pointer store/return, export, and management-RX consumer.
Role: Publish or clear the OMCI MIC validation callback.
Inputs/outputs: Callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes `omci_mic_check`; no MMIO.
Concurrency/ownership: Unsynchronized publication with no lifetime/refcount
handling.
Evidence: Complete three-instruction body, no internal callers, runtime export,
and CPU-management-RX consumer xref.
Open questions: Callback provider/lifetime and validation status contract.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0af84-register_omci_mic_check_handle.md`.

### 0x0af90 register_woe_recycle_handle
Status: complete
Confidence: verified slot-0 store/return, export, and recycle dispatch
ABI/consumer; external callback behavior remains unknown.
Role: Publish or clear IDM recycle callback slot 0.
Inputs/outputs: Recycle callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes `idm_recycle_cb[0]`; no MMIO.
Concurrency/ownership: Unsynchronized publication can race release dispatch;
provider owns callback lifetime.
Evidence: Complete three-instruction body, no internal callers, runtime export,
adjacent slot setters, and slot-0 dispatch from `net_check_reorder_rls_nolock`.
Open questions: Callback provider/lifecycle and completion-ring processing.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0af90-register_woe_recycle_handle.md`.

### 0x0af9c register_woe1_recycle_handle
Status: complete
Confidence: verified slot-1 store/return, export, and recycle dispatch
ABI/consumer; external callback behavior remains unknown.
Role: Publish or clear IDM recycle callback slot 1.
Inputs/outputs: Recycle callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes `idm_recycle_cb[1]`; no MMIO.
Concurrency/ownership: Unsynchronized publication can race release dispatch;
provider owns callback lifetime.
Evidence: Complete three-instruction body, exact `+8` slot target, no internal
callers, runtime export, and slot-1 reorder-release dispatch.
Open questions: Callback provider/lifecycle and slot-specific processing.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0af9c-register_woe1_recycle_handle.md`.

### 0x0afa8 register_woe2_recycle_handle
Status: complete
Confidence: verified slot-2 store/return, export, and recycle dispatch
ABI/consumer; external callback behavior remains unknown.
Role: Publish or clear IDM recycle callback slot 2.
Inputs/outputs: Recycle callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes `idm_recycle_cb[2]`; no MMIO.
Concurrency/ownership: Unsynchronized publication can race release dispatch;
provider owns callback lifetime.
Evidence: Complete three-instruction body, exact `+0x10` slot target, no internal
callers, runtime export, and conditional slot-2 reorder-release dispatch.
Open questions: Callback provider/lifecycle and slot-specific processing.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0afa8-register_woe2_recycle_handle.md`.

### 0x0afb4 register_wlan_to_essid_handle
Status: complete
Confidence: verified private-slot store/return, export, and `np.ko` registration
caller; callback ABI is only partly evidenced.
Role: Publish or clear a raw WLAN-name/ESSID mapping callback slot.
Inputs/outputs: Raw callback pointer; returns that same pointer.
Globals/MMIO/callbacks: Writes private `idm_wlanname_to_essid`; no MMIO.
Concurrency/ownership: Unsynchronized raw-pointer publication with no lifetime
or reference handling.
Evidence: Complete three-instruction body, no internal callers/other slot uses,
runtime export, and `np.ko` `tm_initial` registration of
`aclDevNameToWlanIDMMap`.
Open questions: Exact callback ABI/consumer and why the private slot is otherwise
unreferenced in this build.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0afb4-register_wlan_to_essid_handle.md`.

### 0x0b010 get_next_rxdesc
Status: complete
Confidence: verified base/index/stride computation, prefetch, producer wrap,
callers, and configuration-global use; queue field labels are strong inference.
Role: Return and prefetch the current RX descriptor, then advance the queue's
producer index with non-power-of-two-safe wrap logic.
Inputs/outputs: RX queue record; returns its current descriptor pointer.
Globals/MMIO/callbacks: Reads exported `uNPPT_IDM_DESC_MODE`; updates queue
producer at `+0x8`; no MMIO or callbacks.
Concurrency/ownership: No local synchronization or ownership transfer; caller
must serialize access to a queue record.
Evidence: Complete 16-instruction body, two direct RX callers, no callees, and
IDM queue getter/initializer layout cross-check.
Open questions: Descriptor-mode semantics/valid range and per-queue
serialization guarantee.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b010-get_next_rxdesc.md`.

### 0x0b050 net_check_reorder_rls_nolock
Status: complete
Confidence: verified slot iteration, count clamp, callback context/dispatch,
index/counter updates, ops-table submission, callers, and lock context; context
field labels are strong inference.
Role: Service IDM reorder-release counts and dispatch registered recycle hooks.
Inputs/outputs: No semantic inputs or return value.
Globals/MMIO/callbacks: Reads release slots through CPU-net ops `+0x58`, invokes
`idm_recycle_cb[0..2]`, updates release indices/counters, and submits three
counts through ops `+0x60`; no direct MMIO.
Concurrency/ownership: No local lock; both direct callers hold `idm_lock_tx`.
Callback publication remains unsynchronized and context is stack-local.
Evidence: Complete 84-instruction body, two lock-protected callers, direct
ops-table target analysis, completion-ring initialization, and setter cross-check.
Open questions: Callback providers/lifetimes and hardware release-ring semantics.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0b050-net_check_reorder_rls_nolock.md`.

### 0x0b1a0 dev_kfree_skb_any
Status: complete
Confidence: verified fixed forwarding call, argument value, imported target, all
in-module callers, and semantically unused residual return register.
Role: Local skb-release wrapper that passes fixed argument `1` to the imported
kernel helper.
Inputs/outputs: skb pointer input; no semantic return value.
Globals/MMIO/callbacks: Calls imported `__dev_kfree_skb_any(skb, 1)`; no globals,
MMIO, or callbacks.
Concurrency/ownership: No local synchronization; transfers skb ownership to the
imported helper.
Evidence: Complete six-instruction body, 13 direct callers, imported target, and
runtime kallsyms.
Open questions: Vendor-kernel ABI meaning of fixed helper argument `1`.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0b1a0-dev_kfree_skb_any.md`.

### 0x0b1b8 napi_complete
Status: complete
Confidence: verified fixed forwarding call, argument value, imported target, all
direct callers, and semantically unused residual return register.
Role: Local NAPI-completion wrapper that passes fixed `work_done = 0`.
Inputs/outputs: NAPI pointer input; no semantic return value.
Globals/MMIO/callbacks: Calls imported `napi_complete_done(napi, 0)`; no globals,
MMIO, or callbacks.
Concurrency/ownership: No local synchronization; NAPI state management belongs
to the imported helper and call sites.
Evidence: Complete six-instruction body, four direct poll callers, imported
target, and runtime kallsyms.
Open questions: Vendor-kernel semantics of fixed zero `work_done`.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b1b8-napi_complete.md`.

### 0x0b2ec dump_net_condition_set
Status: complete
Confidence: verified argument truncation, condition-record writes, byte swaps,
print-mode messages, export caller, and predicate consumer; field labels are
strong inference from observed use.
Role: Configure two raw packet-dump predicates and their match-selection mode.
Inputs/outputs: Low-byte print type/index plus mask, value, and shift; no
semantic return value.
Globals/MMIO/callbacks: Writes `g_net_dump_select` and one 24-byte condition
record; no MMIO or callbacks.
Concurrency/ownership: No synchronization; settings can race packet-path
predicate readers.
Evidence: Complete 105-instruction body, direct byte-swap calls, `np.ko`
configuration-store caller, and `dump_net_check` cross-check.
Open questions: Live-update synchronization and external out-of-range inputs.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0b2ec-dump_net_condition_set.md`.

### 0x0b700 cpu_timer_unlock
Status: complete
Confidence: verified timer-index arithmetic, queue selection, reclaim call,
expiry update, target CPU, rearm behavior, and registration context; timer
layout labels are strong inference.
Role: Periodically reclaim the unlock queue associated with one per-slot timer
and reschedule that timer on the IPsec TX CPU.
Inputs/outputs: Timer callback pointer; no semantic return value.
Globals/MMIO/callbacks: Uses `cpu_unlock_timer`, `unlock_tq`, `jiffies`, and
`ipsec_tx_cpu`; calls completion reclaim and `add_timer_on`; no MMIO.
Concurrency/ownership: No local lock; caller/timer subsystem must serialize the
queue scan.
Evidence: Complete 24-instruction body, timer-registration xrefs, and direct
callee/global references.
Open questions: Two-slot queue alias rationale and external scan serialization.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0b700-cpu_timer_unlock.md`.

### 0x0b768 do_raw_spin_lock
Status: complete
Confidence: verified prefetch, acquire/exclusive fast path, contention test,
slow-path arguments, callers, and semantically unused residual return value.
Role: Acquire a raw 32-bit TX/NAPI lock through a local fast path or kernel slow
path.
Inputs/outputs: Lock-word pointer; no semantic return value.
Globals/MMIO/callbacks: Calls imported
`queued_spin_lock_slowpath(lock, observed, 0, 1)` only on contention; no globals
or MMIO.
Concurrency/ownership: Acquires the supplied lock; caller owns compatible
store-release unlock.
Evidence: Complete 15-instruction body, atomic instruction sequence, import
arguments, and nine direct callers.
Open questions: Vendor slow-path argument semantics and external unlock ABI.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0b768-do_raw_spin_lock.md`.

### 0x0bae4 dump_net_check
Status: complete
Confidence: verified mode dispatch, bounds checks, two 64-bit masked
comparisons, return convention, globals, and all direct callers; condition field
labels are strong inference from paired setter/use.
Role: Decide whether a packet should pass the configured debug-dump filter.
Inputs/outputs: Data pointer and length; returns 0 for acceptance and -1 for
rejection or required-read bounds failure.
Globals/MMIO/callbacks: Reads debug selection and two condition records; no
writes, MMIO, or callbacks.
Concurrency/ownership: No synchronization; can observe a concurrent condition
update partway through its fields.
Evidence: Complete 83-instruction body, paired setter reconstruction, and nine
direct RX/trap/TX callers.
Open questions: Payload-endianness contract and null-pointer caller guarantee.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0bae4-dump_net_check.md`.

### 0x0bc30 dump_net_data
Status: complete
Confidence: verified length cap, byte formatting, line cadence, final newline,
all direct callers, and semantically unused residual return register.
Role: Emit a bounded hexadecimal debug dump of packet data.
Inputs/outputs: Data pointer and length; no semantic return value.
Globals/MMIO/callbacks: Calls printk only; no global state, MMIO, or callbacks.
Concurrency/ownership: No local synchronization or ownership behavior; caller
must validate data and choose debug gating.
Evidence: Complete 33-instruction body, exact format/cap/newline behavior, and
14 direct callers.
Open questions: Vendor use of the `\1c` printk level prefix.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0bc30-dump_net_data.md`.

### 0x0bcb8 dump_net_desc
Status: complete
Confidence: verified raw descriptor reads, all three printk forms, alternate
second-argument branch, callers, and semantically unused residual return
register; field meanings beyond printed labels are unknown.
Role: Print RX/trap descriptor diagnostics in ordinary or trap-detail format.
Inputs/outputs: Raw descriptor pointer and format selector; no semantic return.
Globals/MMIO/callbacks: Calls printk only; no globals, MMIO, or callbacks.
Concurrency/ownership: Read-only diagnostic with no synchronization or ownership
behavior.
Evidence: Complete 53-instruction body, exact strings/raw loads, and five direct
debug callers.
Open questions: Descriptor field semantics and nonzero format selector meaning.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0bcb8-dump_net_desc.md`.

### 0x0bd8c idm_set_wifi_trap_info
Status: complete
Confidence: verified descriptor/output/debug/counter/return behavior; output
field labels are raw where semantics remain unknown.
Role: Build stack-local Wi-Fi trap metadata from one RX descriptor and perform
reason-selected diagnostics.
Inputs/outputs: Descriptor pointer, output record, and queue input; writes 36
bytes of metadata and returns raw descriptor byte `+0x7`, ignored by all callers.
Globals/MMIO/callbacks: Reads `memstart_addr` and debug budgets; updates three
raw counters; calls dump helpers; no direct MMIO. Callers pass the record to
`idm_skb_recv`.
Concurrency/ownership: No validation or synchronization. Caller owns the stack
record; callback lifetime and scratch-tail contract remain external.
Evidence: Complete 105-instruction body, three direct caller xrefs, caller
decompilations, and paired dump-helper reconstructions.
Open questions: Output-field/reason semantics, queue-15 flag, and receiver ABI.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0bd8c-idm_set_wifi_trap_info.md`.

### 0x0bf3c cpu_dev_stat
Status: complete
Confidence: verified null/sentinel/offset behavior and caller set; returned
statistics-record type is a strong inference.
Role: Return the caller-visible netdev statistics subrecord when its input is
valid under this helper's exact raw guard.
Inputs/outputs: Returns null for null or exact `(void *)-0x880`; otherwise
returns `device + 0x890` without dereferencing the input.
Globals/MMIO/callbacks: None.
Concurrency/ownership: No validation, synchronization, allocation, or ownership
transfer; callers own device lifetime and counter synchronization.
Evidence: Complete seven-instruction body and 31 direct call sites across RX,
TX, GSO, test, and netdev-stats paths.
Open questions: Original field type/name and the sentinel's provenance.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0bf3c-cpu_dev_stat.md`.

### 0x0bf58 cpu_eth_get_stats
Status: complete
Confidence: verified direct forwarding behavior, return propagation, and two
static operation-table references.
Role: Expose the CPU-net statistics-record accessor through static operation
tables.
Inputs/outputs: Forwards one device pointer to `cpu_dev_stat` and returns its
result unchanged.
Globals/MMIO/callbacks: No direct global/MMIO/callback access; operation-table
entries at `0x1dd18` and `0x1df38` point to it.
Concurrency/ownership: No local synchronization, allocation, or ownership
behavior; inherits the callee's input guard and caller's lifetime contract.
Evidence: Complete five-instruction body, direct callee, and two data xrefs.
Open questions: Original operation-table field name and complete table layout.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0bf58-cpu_eth_get_stats.md`.

### 0x0c3cc cpu_net_free_buf
Status: complete
Confidence: verified argument forwarding, ops slot, sole caller, and unused
residual return register.
Role: Forward a raw buffer/pool pair to the CPU-net backend free operation.
Inputs/outputs: Takes live ABI arguments `(buffer, pool)` and makes one indirect
call through `cpu_net_ops + 0x30`; semantic return type is void.
Globals/MMIO/callbacks: Reads `cpu_net_ops`; invokes its free-buffer slot; no
direct MMIO or other global writes.
Concurrency/ownership: No local guard, lock, allocation, or policy; backend and
caller own buffer validity/lifetime.
Evidence: Complete eight-instruction body, sole caller, ops-table mapping, and
CPU-local helper cross-check.
Open questions: Backend free ABI/ownership and residual return rationale.
Recovered source: `recovered/plat_cpu_net.c`; detailed record:
`functions/0x0c3cc-cpu_net_free_buf.md`.

### 0x0ba8c __raw_spin_lock_irqsave
Status: complete
Confidence: verified DAIF save/mask sequence, raw acquire/slow path, return,
and five TX callers.
Role: Mask local IRQs as needed, acquire one raw lock, and return prior DAIF
state for caller-side restoration.
Inputs/outputs: Lock pointer input; returns original full DAIF value after
acquiring the lock.
Globals/MMIO/callbacks: Uses CPU system registers and
`queued_spin_lock_slowpath`; no module globals or MMIO.
Concurrency/ownership: Performs acquire lock semantics; caller owns byte-release
unlock and interrupt restoration.
Evidence: Complete 22-instruction body, atomic instruction sequence, and five
direct TX call sites.
Open questions: Context-predicate and slow-path ABI semantics.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0ba8c-raw_spin_lock_irqsave.md`.

### 0x0af5c arch_local_irq_restore
Status: complete
Confidence: verified full DAIF write, residual return register, and all three
direct TX callers.
Role: Restore caller-supplied CPU-local DAIF state after irqsave lock release.
Inputs/outputs: Writes input flags to complete DAIF; semantic return type void.
Globals/MMIO/callbacks: CPU system-register access only.
Concurrency/ownership: No lock or ownership action; caller releases the raw lock
before restoring local interrupt state.
Evidence: Complete three-instruction body, three caller xrefs, and paired
irqsave analysis.
Open questions: Any indirect consumer of the residual return register.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0af5c-arch_local_irq_restore.md`.

### 0x0fb7c arch_local_irq_save
Status: complete
Confidence: verified full DAIF read, conditional IRQ mask, return value, and
two FIFO callers.
Role: Capture complete DAIF state and conditionally mask local IRQs for FIFO
critical sections.
Inputs/outputs: No arguments; returns original DAIF and may set its IRQ mask.
Globals/MMIO/callbacks: CPU system-register access only.
Concurrency/ownership: Does not lock or restore itself; FIFO callers own their
critical sections and later restore the captured state.
Evidence: Complete six-instruction body and two direct FIFO caller xrefs.
Open questions: FIFO locking rules and DAIF-state assumptions.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0fb7c-arch_local_irq_save.md`.

### 0x0fb94 arch_local_irq_restore_0
Status: complete
Confidence: verified full DAIF write, residual return register, and four direct
FIFO callers.
Role: Restore FIFO caller's captured DAIF state through a distinct binary entry.
Inputs/outputs: Writes input flags to complete DAIF; semantic return type void.
Globals/MMIO/callbacks: CPU system-register access only.
Concurrency/ownership: No lock/ownership behavior; caller supplies the saved
state after its FIFO critical operations.
Evidence: Complete three-instruction body, four caller xrefs, and paired save
analysis.
Open questions: Any indirect consumer of the residual return register.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0fb94-arch_local_irq_restore_0.md`.

### 0x0fba0 __my_cpu_offset
Status: complete
Confidence: verified TPIDR_EL1 read, return value, and nine callers;
per-CPU-offset role is strongly inferred from caller arithmetic.
Role: Return the CPU-local TPIDR_EL1 value for per-CPU state address calculation.
Inputs/outputs: No inputs; returns raw TPIDR_EL1.
Globals/MMIO/callbacks: CPU system-register read only.
Concurrency/ownership: No synchronization, allocation, or ownership behavior.
Evidence: Complete two-instruction body and nine direct buffer/FIFO/stack caller
xrefs.
Open questions: Vendor per-CPU layout and TPIDR_EL1 base-vs-offset contract.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0fba0-my_cpu_offset.md`.

### 0x0fba8 do_raw_spin_lock_flags.isra.2
Status: complete
Confidence: verified raw acquire/slow path, fixed arguments, unused residual
return register, and both FIFO callers.
Role: Acquire a FIFO record lock without changing IRQ state.
Inputs/outputs: Lock pointer input; semantic return type void.
Globals/MMIO/callbacks: Calls `queued_spin_lock_slowpath` on contention; no
direct module globals or MMIO.
Concurrency/ownership: Acquires lock with LDAXR/STXR; caller owns low-byte
store-release unlock and DAIF restoration.
Evidence: Complete 15-instruction body and two FIFO caller sequences.
Open questions: FIFO lock field type and slow-path ABI semantics.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0fba8-do_raw_spin_lock_flags_isra_2.md`.

### 0x0fc28 do_raw_spin_lock_0
Status: complete
Confidence: verified machine-code-equivalent raw locking behavior, residual
return, and both FIFO callers.
Role: Acquire a FIFO record lock on the alternate non-IRQ-save path.
Inputs/outputs: Lock pointer input; semantic return type void.
Globals/MMIO/callbacks: Calls `queued_spin_lock_slowpath` on contention; no
direct module globals or MMIO.
Concurrency/ownership: Acquires with LDAXR/STXR; caller owns low-byte
store-release unlock without IRQ restoration on this path.
Evidence: Complete 15-instruction body and two context-split FIFO call sites.
Open questions: Context predicate, FIFO lock type, and slow-path ABI.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0fc28-do_raw_spin_lock_0.md`.

### 0x0fbe4 _buf_fifo_free_data
Status: complete
Confidence: verified three-way selection dispatch, imports/cache global, fixed
reason, residual returns, and all callers.
Role: Release a FIFO-drained object through one of three raw selection paths.
Inputs/outputs: Selection/object inputs; semantic return type void.
Globals/MMIO/callbacks: Reads `kmem_buf_cache` for selection 1; calls imported
release helpers; no MMIO.
Concurrency/ownership: No local synchronization or validation; caller owns FIFO
serialization and object-selection correctness.
Evidence: Complete 17-instruction body and three direct FIFO caller xrefs.
Open questions: Selection meanings/ownership and vendor free reason one.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0fbe4-buf_fifo_free_data.md`.

### 0x0fc64 buf_fifo_free_data
Status: complete
Confidence: verified high/low context split, FIFO/staging layout, batch/direct
paths, no-room release, counter updates, returns, and caller set.
Role: Queue an object for selection-specific FIFO release, batch in high context,
or release it when the ring has no space.
Inputs/outputs: Staging pointer, unchecked selection, and object; returns a raw
per-selection path counter.
Globals/MMIO/callbacks: Uses `buf_fifo`, `buf_fifo_cnt`, locks, DAIF helpers,
and `_buf_fifo_free_data`; no MMIO.
Concurrency/ownership: High context stages 32 entries before locked commit; low
context uses IRQ save plus lock. Full/no-room paths transfer object release.
Evidence: Complete 152-instruction body, three direct caller xrefs, and paired
lock/IRQ/release helper evidence.
Open questions: Original layouts/counter meanings, batch rationale, and
selection ownership contracts.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0fc64-buf_fifo_free_data.md`.

### 0x1003c buf_fifo_alloc_data
Status: complete
Confidence: verified context split, shared FIFO/staging layout, batch/direct
dequeue, counters, null behavior, and four callers.
Role: Return one selection-specific raw FIFO object, optionally through a
high-context staging batch.
Inputs/outputs: Allocation-staging pointer and unchecked selection; returns an
object pointer or null.
Globals/MMIO/callbacks: Uses `buf_fifo`, `buf_fifo_cnt`, locks, and DAIF helpers;
no MMIO or callback.
Concurrency/ownership: High context refills a local 32-entry batch; low context
uses IRQ save plus lock. Does not free/own returned object.
Evidence: Complete 152-instruction body, four caller xrefs, and paired enqueue
analysis.
Open questions: Staging prefix/counter meanings, null slots, and caller
ownership contracts.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x1003c-buf_fifo_alloc_data.md`.

### 0x1029c idm_skb_stack_pop
Status: complete
Confidence: verified selector/FIFO mapping, recycle/status/size/free path, and
zero direct code xrefs.
Role: Pop, recycle, size-check, and return one IDM skb-stack entry.
Inputs/outputs: Selector low bit and signed minimum size; returns recycled skb
or null.
Globals/MMIO/callbacks: Uses two per-CPU staging bases, FIFO allocator,
`skb_recycle`, and a two-element short-buffer counter; no MMIO.
Concurrency/ownership: FIFO synchronization is delegated; short skb failure
transfers ownership to fixed-reason skb release.
Evidence: Complete 44-instruction body, zero direct caller xrefs, and paired
push/FIFO analysis.
Open questions: External ABI/caller, skb bit/recycle semantics, staging ownership.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x1029c-idm_skb_stack_pop.md`.

### 0x10354 net_alloc_skb
Status: complete
Confidence: verified per-CPU staging computation, FIFO 0 forwarding, return
propagation, and sole IDM-RX caller.
Role: Obtain an skb head from the selection-0 FIFO allocator.
Inputs/outputs: No inputs; returns raw FIFO object or null.
Globals/MMIO/callbacks: Reads `skb_free_data` and TPIDR_EL1; calls FIFO allocator;
no MMIO.
Concurrency/ownership: FIFO synchronization and object ownership are delegated.
Evidence: Complete ten-instruction body and sole direct caller xref.
Open questions: Staging type and empty-FIFO ownership contract.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x10354-net_alloc_skb.md`.

### 0x10380 net_alloc_kmem
Status: complete
Confidence: verified per-CPU staging computation, FIFO 1 forwarding, return
propagation, and sole IDM-backend caller.
Role: Obtain a raw allocation candidate from FIFO 1.
Inputs/outputs: No inputs; returns raw FIFO object or null.
Globals/MMIO/callbacks: Reads `kmem_free_data` and TPIDR_EL1; calls FIFO allocator;
no MMIO.
Concurrency/ownership: FIFO synchronization and object ownership are delegated.
Evidence: Complete ten-instruction body and sole direct caller xref.
Open questions: Staging type and empty-FIFO ownership contract.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x10380-net_alloc_kmem.md`.

### 0x103ac net_free_kmem
Status: complete
Confidence: verified object forwarding, per-CPU staging/FIFO 1 selection, return
propagation, and sole IDM-backend caller.
Role: Return a raw object through selection-1 FIFO staging.
Inputs/outputs: Object input; returns the FIFO free helper's raw counter.
Globals/MMIO/callbacks: Reads `kmem_free_data` and TPIDR_EL1; calls FIFO helper;
no MMIO.
Concurrency/ownership: FIFO synchronization and release ownership are delegated.
Evidence: Complete 12-instruction body and sole direct caller xref.
Open questions: Staging type and propagated counter meaning.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x103ac-net_free_kmem.md`.

### 0x103dc idm_skb_stack_wifi_push
Status: complete
Confidence: verified priority flag dispatch, selectors, fallback release,
residual return, and sole completion-reclaim caller.
Role: Route an IDM TX completion skb to FIFO 2/3 recycling or fixed-reason free.
Inputs/outputs: skb input; semantic return type void.
Globals/MMIO/callbacks: Reads skb word `+0x114`; calls stack push or skb free;
no direct globals/MMIO.
Concurrency/ownership: Transfers skb ownership to selected recycler/free helper;
has no local lock.
Evidence: Complete 14-instruction body and sole TX-completion caller xref.
Open questions: Flag-bit semantics and stack-push ownership policy.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x103dc-idm_skb_stack_wifi_push.md`.

### 0x0fec4 _idm_skb_stack_push
Status: complete
Confidence: verified reject predicates/counters, selection/staging/FIFO flow,
release behavior, return, sole direct in-module caller, and export-table entry.
Role: Validate one skb for FIFO 2/3 recycle or release it with fixed reason.
Inputs/outputs: skb and selector inputs; always returns zero.
Globals/MMIO/callbacks: Uses recycle threshold, four reject counters, per-CPU
staging bases, FIFO helper, and skb free; no MMIO.
Concurrency/ownership: Reject path transfers skb to free; success transfers to
FIFO. No local synchronization protects raw fields/counters.
Evidence: Complete 65-instruction body, sole direct Wi-Fi-push caller, and
`__ksymtab__idm_skb_stack_push` export entry.
Open questions: Raw field/counter meanings and FIFO 2/3 ownership contract.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0fec4-idm_skb_stack_push.md`.

### 0x0ffd8 idm_skb_stack_push
Status: complete
Confidence: verified flag predicate, CPU-net/FIFO arguments, ownership paths,
per-CPU staging, raw return-register behavior, and export-table entry; void
signature is a strong inference.
Role: Return a qualifying skb's raw head to CPU-net then stage the skb in FIFO
0, or free the skb when its two required status bits are absent.
Inputs/outputs: skb input; semantic return type void, with no normalized machine
return value.
Globals/MMIO/callbacks: Reads skb word `+0x114` and pointer `+0x128`, uses
`skb_free_data`, TPIDR_EL1, CPU-net free operation, FIFO helper, and skb free;
no direct MMIO.
Concurrency/ownership: Success sends head then skb to separate backend/FIFO
paths; failure sends the skb to fixed-reason free. Synchronization is delegated.
Evidence: Complete 24-instruction body, no direct in-module code xrefs, and
`__ksymtab_idm_skb_stack_push` export entry.
Open questions: Status-bit meanings, backend ownership contract, and external
ABI return expectation.
Recovered source: `recovered/plat_cpu_tx.c`; detailed record:
`functions/0x0ffd8-idm_skb_stack_push.md`.

### 0x129c8 nppt_smac_init
Status: complete
Confidence: verified control flow, raw register writes, callback initialization,
XMAC mode table, thread launch, fixed zero return, caller, and runtime sequence;
hardware field/type names remain strong inferences or unknown.
Role: Initialize SMAC callback/state arrays and MAC blocks, choose XMAC work
modes, then launch the PHY polling worker.
Inputs/outputs: No semantic inputs; always returns zero and discards all child
statuses.
Globals/MMIO/callbacks: Sets max MAC index, callback arrays, PHY state/type
globals, raw `nppt_base` SMAC/SOPC registers, and calls PHY/XMAC/thread helpers.
Concurrency/ownership: No local lock, allocation, or unwind. Callback arrays are
populated before the worker is created.
Evidence: Complete 353-instruction body, sole `nppt_init` caller, direct callee
disassembly, later callback use, and matching runtime dmesg.
Open questions: Register/PHY-type meanings, callback ABI ownership, and thread
cleanup.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x129c8-nppt_smac_init.md`.

### 0x128ec smac_thread_init
Status: complete
Confidence: verified thread-creation arguments, error test, wakeup/log paths,
sole caller, and raw return behavior; semantic void signature is a strong
inference.
Role: Create and wake the periodic SMAC PHY polling worker.
Inputs/outputs: No semantic inputs; caller ignores residual `printk` return
register values.
Globals/MMIO/callbacks: References only the worker entry, imported thread APIs,
and fixed task-name/diagnostic strings; no mutable module global or MMIO write.
Concurrency/ownership: Creates a task then immediately wakes it on success; no
task pointer is retained locally and no cleanup is attempted on failure.
Evidence: Complete 18-instruction body, one `nppt_smac_init` caller xref, worker
and imported API references, and matching runtime success log.
Open questions: Worker-task retention/stopping and original error-pointer macro.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x128ec-smac_thread_init.md`.

### 0x12890 smac_check_phy_task_thread
Status: complete
Confidence: verified low-byte stop predicate, ordered MAC 0-6 polling, sleep,
return propagation, callback registration, and no local state.
Role: Periodically poll PHY state for all seven MAC indexes.
Inputs/outputs: Unused thread argument; returns the raw stop-query result when
its low byte is nonzero.
Globals/MMIO/callbacks: Calls only stop query, `check_phy`, and interruptible
sleep; no direct globals/MMIO.
Concurrency/ownership: Thread context with no local synchronization; `check_phy`
owns PHY state synchronization and policy.
Evidence: Complete 23-instruction body and callback references from
`smac_thread_init`.
Open questions: Vendor stop-result range and worker stop/join lifecycle.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x12890-smac_check_phy_task_thread.md`.

### 0x126e4 check_phy
Status: complete
Confidence: verified callback/state handling, changed-state paths, MAC/XMAC
guards, helper arguments, cache update, and all seven worker callers; semantic
void return is a strong inference.
Role: Poll one raw PHY callback and apply link-state change to SMAC or XMAC.
Inputs/outputs: MAC byte input; semantic void result with path-dependent
residual helper registers.
Globals/MMIO/callbacks: Uses enable byte, PHY callback/state/identity arrays,
XMAC type globals, and SMAC/XMAC configuration helpers; no direct MMIO access.
Concurrency/ownership: Runs in PHY worker context without local locks; callback
and hardware helper contracts own synchronization and lifetime.
Evidence: Complete 107-instruction body, seven worker caller xrefs, direct
callee analysis, and matching runtime link-change messages.
Open questions: Full status encoding, global meanings, and callback lifetime.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x126e4-check_phy.md`.

### 0x18460 xmac_init
Status: complete
Confidence: verified PHY-type branch, ordered setup calls, independent mode
gates, status OR, diagnostics, return, and sole caller.
Role: Coordinate XMAC PHY setup and selected XMAC0/XMAC1 work-mode programming.
Inputs/outputs: Two raw work-mode inputs; returns the OR of enabled mode-helper
statuses.
Globals/MMIO/callbacks: Reads PHY type and three raw gate/log globals; delegates
all hardware and PHY state changes to callees.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 52-instruction body, one `nppt_smac_init` caller, and direct
analysis of all setup callees.
Open questions: PHY type/gate meanings and delegated mode-programming details.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18460-xmac_init.md`.

### 0x17da0 xmac_init_by_work_mode
Status: complete
Confidence: verified selector truncation, pre-switch calls, all ten setters,
post-setter sequence, raw SOPC write, status/cache behavior, return, and both
callers.
Role: Reset and configure one XMAC for a selected raw work mode.
Inputs/outputs: Byte-truncated XMAC selector and raw mode; returns setter status,
zero on success, or `-1` for an unsupported mode.
Globals/MMIO/callbacks: Calls XPCS/XMAC setup helpers, writes one raw SOPC word,
updates `sg_xmac_work_mode` only on success, and enables TX/RX.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 95-instruction body and two `xmac_init` caller xrefs.
Open questions: SOPC/delay semantics, mode-setter internals, and selector bounds.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17da0-xmac_init_by_work_mode.md`.

### 0x1718c xmac_10gbase_r_conf
Status: complete
Confidence: verified selector validation, CPU-specific sequence, result
aggregation, bypass predicate, cache write, and all callers.
Role: Configure one valid XMAC for raw 10GBASE-R work mode zero.
Inputs/outputs: Byte selector at most four; returns setup status or `-1` on an
invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, XPCS, MAC-config, SerDes, delay, and
bypass helpers; writes the XMAC work-mode cache to zero on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 58-instruction body and three direct caller xrefs.
Open questions: Bypass/delay meanings and CPU-132 ordering rationale.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1718c-xmac_10gbase_r_conf.md`.

### 0x17280 xmac_5gbase_r_conf
Status: complete
Confidence: verified selector validation, CPU-specific sequence, result
aggregation, bypass predicate, cache write, and all callers.
Role: Configure one valid XMAC for raw 5GBASE-R work mode one.
Inputs/outputs: Byte selector at most four; returns setup status or `-1` on an
invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, XPCS, MAC-config, SerDes, delay, and
bypass helpers; writes the XMAC work-mode cache to one on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 60-instruction body and three direct caller xrefs.
Open questions: SerDes/speed meanings and CPU-132 ordering rationale.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17280-xmac_5gbase_r_conf.md`.

### 0x1781c xmac_1gbase_x_conf
Status: complete
Confidence: verified selector validation, CPU-specific sequence, XPCS arguments,
result aggregation, bypass predicate, cache write, and all callers.
Role: Configure one valid XMAC for raw 1GBASE-X work mode two.
Inputs/outputs: Byte selector at most four; returns setup status or `-1` on an
invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, XPCS, MAC-config, SerDes, delay, and
bypass helpers; writes the XMAC work-mode cache to two on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 69-instruction body and three direct caller xrefs.
Open questions: XPCS/SerDes argument meanings and CPU-specific bypass policy.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1781c-xmac_1gbase_x_conf.md`.

### 0x16ee4 xmac_sgmii_conf
Status: complete
Confidence: verified byte truncation, all arguments, CPU-specific order/status
OR, bypass predicate, cache write, and all five callers.
Role: Configure one valid XMAC for raw SGMII work mode three.
Inputs/outputs: Byte-truncated selector/auto-negotiation plus two raw PCS values;
returns aggregate setup status or `-1` for an invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, PCS, MAC-config, SerDes, delay, and bypass
helpers; writes work-mode cache three on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 82-instruction body and five direct caller xrefs.
Open questions: PCS argument meanings and direct caller tuple policy.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16ee4-xmac_sgmii_conf.md`.

### 0x17378 xmac_2pt5gbase_x_conf
Status: complete
Confidence: verified selector validation, CPU-specific sequence, result
aggregation, bypass predicate, cache write, and all callers.
Role: Configure one valid XMAC for raw 2.5GBASE-X work mode four.
Inputs/outputs: Byte selector at most four; returns setup status or `-1` on an
invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, XPCS, MAC-config, SerDes, delay, and
bypass helpers; writes work-mode cache four on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 65-instruction body and four direct caller xrefs.
Open questions: Speed/SerDes meanings and CPU-129 bypass policy.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17378-xmac_2pt5gbase_x_conf.md`.

### 0x17484 xmac_10g_usxgmii_auto_conf
Status: complete
Confidence: verified selector validation, CPU-specific sequence, PCS/auto
arguments, result aggregation, bypass predicate, cache write, and all callers.
Role: Configure one valid XMAC for raw 10G USXGMII auto work mode five.
Inputs/outputs: Byte selector at most four; returns aggregate setup status or
`-1` on an invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, PCS, auto-negotiation, MAC-config, SerDes,
delay, and bypass helpers; writes work-mode cache five on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 73-instruction body and three direct caller xrefs.
Open questions: USXGMII argument meanings and CPU-129 bypass exclusion.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17484-xmac_10g_usxgmii_auto_conf.md`.

### 0x175b0 xmac_5g_usxgmii_auto_conf
Status: complete
Confidence: verified selector validation, CPU-specific sequence, PCS/auto
arguments, result aggregation, bypass predicate, cache write, and all callers.
Role: Configure one valid XMAC for raw 5G USXGMII auto work mode six.
Inputs/outputs: Byte selector at most four; returns aggregate setup status or
`-1` on an invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, PCS, auto-negotiation, MAC-config, SerDes,
delay, and bypass helpers; writes work-mode cache six on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 73-instruction body and three direct caller xrefs.
Open questions: USXGMII/SerDes meanings and CPU-129 bypass exclusion.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x175b0-xmac_5g_usxgmii_auto_conf.md`.

### 0x176dc xmac_2pt5g_usxgmii_auto_conf
Status: complete
Confidence: verified selector validation, CPU-specific sequence, PCS/auto
arguments, result aggregation, bypass predicate, cache write, and all callers.
Role: Configure one valid XMAC for raw 2.5G USXGMII auto work mode seven.
Inputs/outputs: Byte selector at most four; returns aggregate setup status or
`-1` on an invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, PCS, auto-negotiation, MAC-config, SerDes,
delay, and bypass helpers; writes work-mode cache seven on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 78-instruction body and three direct caller xrefs.
Open questions: USXGMII/SerDes meanings and CPU-specific bypass policy.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x176dc-xmac_2pt5g_usxgmii_auto_conf.md`.

### 0x17938 xmac_hsgmii_conf
Status: complete
Confidence: verified byte truncation, variant forwarding, CPU-specific sequence,
result aggregation, bypass predicate, cache write, and all caller xrefs.
Role: Configure one valid XMAC for raw HSGMII work modes eight/nine.
Inputs/outputs: Byte selector/variant; returns setup status or `-1` on an
invalid selector.
Globals/MMIO/callbacks: Calls TX/RX, PCS, MAC-config, SerDes, delay, and bypass
helpers; writes work-mode cache eight on every valid path.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 68-instruction body and four direct caller xrefs.
Open questions: Variant meanings and shared mode-eight cache behavior.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17938-xmac_hsgmii_conf.md`.

### 0x17bd8 xmac_mode_set
Status: complete
Confidence: verified input limits/truncations, speed table, all dispatches,
post-setter speed selection, direct returns, and all callers.
Role: Convert a PHY-facing PCS mode and byte parameters into XMAC setup calls.
Inputs/outputs: XMAC byte, raw PCS mode, input-speed byte, configuration byte;
returns delegated setter status or `-1` for unsupported modes.
Globals/MMIO/callbacks: Reads a six-byte speed table and delegates all hardware
changes to XMAC setters plus `xmac_set_speed_sel`; no direct MMIO/global write.
Concurrency/ownership: No local lock, allocation, or cleanup.
Evidence: Complete 85-instruction body and three `phy_zxic051_check` caller xrefs.
Open questions: PCS mode/table/config meanings and caller serialization.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17bd8-xmac_mode_set.md`.

### 0x1c0c0 phy_zxic051_check
Status: complete
Confidence: verified two-slot selection, callback outputs, mode history/replay,
link and speed recovery thresholds, return encoding, and callback registration.
Role: Drive runtime PHY-051 link/mode/speed reconciliation for XMAC slots 0/1.
Inputs/outputs: PHY byte input; returns `-1` during link/recovery states or a
speed-plus-duplex encoded status when fully reconciled.
Globals/MMIO/callbacks: Uses PHY callback outputs, mode history, link/retry
counters, threshold global, and `xmac_mode_set`; no direct MMIO.
Concurrency/ownership: Shared callback context and counters have no local lock;
mode recovery may reprogram XMAC state.
Evidence: Complete 189-instruction body, callback data references, and three
internal `xmac_mode_set` sites.
Open questions: Raw callback/mode/threshold semantics and synchronization.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1c0c0-phy_zxic051_check.md`.

### 0x1c00c phy_051_set_xmac_speed
Status: complete
Confidence: verified speed gate, two raw PCS-mode branches, conversion and
speed-selection calls, no-op paths, void return, and sole caller.
Role: Apply PHY-051 runtime speed reconciliation for raw PCS modes three/six.
Inputs/outputs: XMAC byte, unsigned speed model, raw PCS mode; no meaningful
return value. The sole caller supplies a zero-extended speed byte.
Globals/MMIO/callbacks: No direct global/MMIO write; delegates to speed mapping,
speed selection, or SGMII auto-mode processing.
Concurrency/ownership: No local lock, allocation, or ownership transfer.
Evidence: Complete 45-instruction body, all direct callees, and one caller xref.
Open questions: Original speed declaration width and raw PCS-mode meanings.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1c00c-phy_051_set_xmac_speed.md`.

### 0x1bfa4 phy_051_set_xmac_work_mode
Status: complete
Confidence: verified both raw PCS-mode branches, delegated arguments,
unconditional mode-six speed selection, default return, and absence of callers.
Role: Configure a limited PHY-051 XMAC work-mode subset for raw PCS modes three
and six.
Inputs/outputs: XMAC byte and raw PCS mode; returns delegated configuration
status or zero for all other modes.
Globals/MMIO/callbacks: No direct global/MMIO write; delegates XMAC mode and
speed selection.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 26-instruction body and an exhaustive xref query with no
external reference.
Open questions: Unreferenced status and raw PCS/argument meanings.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1bfa4-phy_051_set_xmac_work_mode.md`.

### 0x17fdc xmac_switch_uni_speed_to_xmac_speed
Status: complete
Confidence: verified all six mappings, mode-eight/nine special handling,
no-write default, void return, and every direct caller.
Role: Convert a UNI speed code to the XMAC speed-select encoding.
Inputs/outputs: XMAC byte, unsigned UNI speed, output-word pointer; only inputs
one through six write the output pointer.
Globals/MMIO/callbacks: Reads `sg_xmac_work_mode[xmac]` for UNI speed four; no
direct MMIO or callback.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; output storage belongs to the caller.
Evidence: Complete 29-instruction jump-table body and six direct caller xrefs.
Open questions: Hardware speed encoding and the HSGMII special case.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17fdc-xmac_switch_uni_speed_to_xmac_speed.md`.

### 0x1670c xmac_set_speed_sel
Status: complete
Confidence: verified selector truncation, special and NPPT-relative register
addresses, high-three-bit RMW, void return, and all nine direct callers.
Role: Write the XMAC speed-select field in one selector-specific register.
Inputs/outputs: XMAC byte and unsigned speed; no meaningful return value.
Globals/MMIO/callbacks: Reads `nppt_base` for every selector except two/three;
does a volatile 32-bit RMW with mask `0x1fffffff` and shift 29.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the RMW.
Evidence: Complete 28-instruction body and nine direct caller xrefs.
Open questions: Special raw-address windows and required register serialization.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1670c-xmac_set_speed_sel.md`.

### 0x18058 xmac_speed_process_in_sgmii_auto_mode
Status: complete
Confidence: verified all state/PCS gates, output-pointer ordering/defaults,
speed mapping/write, logging, void return, and sole caller.
Role: Apply a PCS-reported auto-negotiated SGMII speed to one XMAC selector.
Inputs/outputs: XMAC byte; no meaningful return value.
Globals/MMIO/callbacks: Reads `g_xmac_work_in_auto` and `sg_xmac_work_mode`,
queries PCS speed/duplex/status, and delegates the RMW to XMAC speed selection.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 52-instruction body, complete PCS-reader ABI analysis, and
one direct caller xref.
Open questions: Exact auto flag, mode-three, and status-bit semantics.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18058-xmac_speed_process_in_sgmii_auto_mode.md`.

### 0x18530 xmac_speed_process_in_usxgmii_auto_mode
Status: complete
Confidence: verified all state/PCS gates, output-pointer ordering/defaults,
speed mapping/write, logging, void return, and absence of callers.
Role: Apply a PCS-reported auto-negotiated USXGMII speed to one XMAC selector.
Inputs/outputs: XMAC byte; no meaningful return value.
Globals/MMIO/callbacks: Reads `g_xmac_work_in_auto` and `sg_xmac_work_mode`,
queries PCS speed/duplex/status, and delegates the RMW to XMAC speed selection.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 53-instruction body, complete PCS-reader ABI analysis, and
an exhaustive xref query with no external reference.
Open questions: Unreferenced status, auto flag, mode-five-through-seven, and
status-bit semantics.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18530-xmac_speed_process_in_usxgmii_auto_mode.md`.

### 0x1860c xmac_speed_process
Status: complete
Confidence: verified auto gate, mode routing, two PCS query paths, local-output
handling, speed writes/logging, void return, and sole caller.
Role: Reconcile an auto-negotiated SGMII or USXGMII speed for one XMAC selector.
Inputs/outputs: XMAC byte; no meaningful return value.
Globals/MMIO/callbacks: Reads `g_xmac_work_in_auto` and `sg_xmac_work_mode`,
queries PCS state, and delegates XMAC speed selection.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 78-instruction body, verified PCS ABI usage, and one direct
caller xref.
Open questions: Reason for duplicate narrow-helper logic and auto-state meaning.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1860c-xmac_speed_process.md`.

### 0x18130 xmac_config_speed_duplex
Status: complete
Confidence: verified speed mapping, duplex fast path, CPU-133 gate sequence,
five-call reconfiguration order, void return, and sole caller.
Role: Apply a PHY-reported XMAC speed/duplex update, with reset only when
duplex changes.
Inputs/outputs: XMAC byte, raw unsigned speed model, requested duplex; no
meaningful return value.
Globals/MMIO/callbacks: Delegates mapping, duplex read/write, reset/config,
SOPC send-enable, and CPU-133 auto-gate management.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 68-instruction body and one direct `check_phy` caller xref.
Open questions: SOPC gate purpose and original speed-argument width.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18130-xmac_config_speed_duplex.md`.

### 0x16c84 xmac_get_duplex_mode
Status: complete
Confidence: verified selector truncation, special/NPPT-relative register
addresses, bit-24 normalization, void return, and sole caller.
Role: Read and normalize one XMAC duplex indication into caller-owned storage.
Inputs/outputs: XMAC byte and output-word pointer; no meaningful return value.
Globals/MMIO/callbacks: Reads one selector-specific volatile 32-bit register;
reads `nppt_base` except for selectors two/three.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; output storage belongs to the caller.
Evidence: Complete 22-instruction body and one direct caller xref.
Open questions: Bit polarity/hardware identity and special raw-address windows.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16c84-xmac_get_duplex_mode.md`.

### 0x16bfc xmac_set_duplex_mode
Status: complete
Confidence: verified selector handling, shared getter/setter address formulas,
bit-24 RMW polarity, void return, and all three direct callers.
Role: Write a normalized duplex input into a selector-specific XMAC field.
Inputs/outputs: XMAC byte and unsigned duplex input; no meaningful return.
Globals/MMIO/callbacks: Volatile RMW on the getter's selector-specific register;
reads `nppt_base` except for selectors two/three.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the RMW.
Evidence: Complete 34-instruction body and three direct caller xrefs.
Open questions: Bit hardware identity/polarity and required synchronization.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16bfc-xmac_set_duplex_mode.md`.

### 0x17d38 xmac_set_sopc_duplex_mode
Status: complete
Confidence: verified selector validation/diagnostic, bit selection/polarity,
void return, and both direct callers.
Role: Update one selector-specific SOPC duplex-control bit in a shared word.
Inputs/outputs: XMAC byte and unsigned duplex input; no meaningful return.
Globals/MMIO/callbacks: Volatile RMW at `nppt_base + 0x343f0`, bit `xmac + 4`.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the RMW.
Evidence: Complete 26-instruction body and two direct caller xrefs.
Open questions: SOPC bit hardware identity/polarity and synchronization.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17d38-xmac_set_sopc_duplex_mode.md`.

### 0x17f24 xmac_sopc_send_enable
Status: complete
Confidence: verified selector truncation, ready/send address formulas,
unbounded polling, delay/log sequence, void return, and sole caller.
Role: Wait for SOPC readiness then enable send processing for one XMAC selector.
Inputs/outputs: XMAC byte; no meaningful return value.
Globals/MMIO/callbacks: Polls and writes NPPT-relative SOPC indexed registers.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; readiness polling can block indefinitely.
Evidence: Complete 46-instruction body and one direct caller xref.
Open questions: Ready/send register semantics and indefinite polling policy.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x17f24-xmac_sopc_send_enable.md`.

### 0x16d6c xamc_init_conf_by_speed
Status: complete
Confidence: verified selector windows, fixed offsets/values, call order, final
RMW, void return, and all 19 direct callers.
Role: Initialize XMAC configuration windows for a selected speed encoding.
Inputs/outputs: XMAC byte and unsigned speed; no meaningful return value.
Globals/MMIO/callbacks: Writes selector-specific volatile register windows and
delegates speed/duplex field setup.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the multi-register initialization.
Evidence: Complete 94-instruction body and 19 direct caller xrefs.
Open questions: Window/value/bit-9 meanings and required synchronization.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16d6c-xamc_init_conf_by_speed.md`.

### 0x169e4 xmac_reset
Status: complete
Confidence: verified low-byte test, both reset masks, delegated call, void
return, and both direct callers.
Role: Select the XMAC reset mask and delegate reset execution to `smac_reset`.
Inputs/outputs: XMAC byte; no meaningful return value.
Globals/MMIO/callbacks: No direct global/MMIO access; delegates to `smac_reset`.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 10-instruction body and two direct caller xrefs.
Open questions: Hardware reset domains represented by `0x400` and `0x800`.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x169e4-xmac_reset.md`.

### 0x12358 smac_reset
Status: complete
Confidence: verified reset-register offset, clear/set sequence, delay,
diagnostics, void return, and all three direct callers.
Role: Pulse selected bits in the shared SMAC/XMAC reset word.
Inputs/outputs: Unsigned reset mask; no meaningful return value.
Globals/MMIO/callbacks: Volatile read/write at `nppt_base + 0x2c0004`.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the shared reset sequence.
Evidence: Complete 33-instruction body and three direct caller xrefs.
Open questions: Reset-domain semantics and required synchronization.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x12358-smac_reset.md`.

### 0x11fe0 sopc_send_enable
Status: complete
Confidence: verified selector split, normal/MAC6 poll limits, register formulas,
delay/log/write order, no-op path, void return, and all callers.
Role: Poll SMAC SOPC readiness and trigger send-enable with MAC6-specific logic.
Inputs/outputs: SMAC byte; no meaningful return value.
Globals/MMIO/callbacks: Reads `g_smac_max_index`, polls/writes NPPT-relative
SOPC registers, and delays through `__const_udelay`.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; wait is bounded but MAC6 has materially longer delay behavior.
Evidence: Complete 99-instruction body and three direct caller xrefs.
Open questions: MAC6 timing/register rationale and timeout-write policy.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x11fe0-sopc_send_enable.md`.

### 0x123ec nppt_smac_config_speed_duplex
Status: complete
Confidence: verified input truncation, config-bit cases, normal/MAC6 windows,
SOPC updates, reset/gate sequence, send-enable tail, void return, and caller.
Role: Reconfigure ordinary SMAC or RGMII speed/duplex state after PHY change.
Inputs/outputs: MAC, speed, and duplex bytes; no meaningful return value.
Globals/MMIO/callbacks: Reads `g_smac_max_index`; updates NPPT/RGMII/SOPC words;
delegates reset, gate management, and send enable.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the multi-register transition.
Evidence: Complete 186-instruction body and one direct `check_phy` caller xref.
Open questions: Config/SOPC bit meanings, MAC6 unconditional NPPT write, and
required synchronization.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x123ec-nppt_smac_config_speed_duplex.md`.

### 0x11fcc sub_11FCC
Status: complete
Confidence: verified all instructions, no-return fall-through, system-register
sequence, target argument source, and absence of external xrefs.
Role: Unreferenced ICC priority-mask fragment that falls through into SOPC send
enable.
Inputs/outputs: No formal input; saved ICC PMR low byte becomes fall-through MAC
input; no meaningful return value.
Globals/MMIO/callbacks: Reads/writes `ICC_PMR_EL1`, issues `DSB SY`, then falls
through to `sopc_send_enable`.
Concurrency/ownership: No allocation, cleanup, or ownership transfer.
Evidence: Complete five-instruction sequence, function boundary at fall-through
target, and no external xrefs.
Open questions: Why this unreferenced priority-mask fragment exists.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x11fcc-sub_11FCC.md`.

### 0x12178 nppt_smac_disable
Status: complete
Confidence: verified MAC bound, normal/RGMII/XMAC branches, config-bit clear,
SOPC update predicate, void return, and sole caller.
Role: Disable a normal SMAC, RGMII MAC6, or delegated XMAC data path on link down.
Inputs/outputs: MAC byte; no meaningful return value.
Globals/MMIO/callbacks: Reads `g_smac_max_index`, updates NPPT/RGMII config and
normal SOPC state, or delegates XMAC TX/RX disable.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls shared state.
Evidence: Complete 54-instruction body and one direct `check_phy` caller xref.
Open questions: Config-bit meanings and MAC6 SOPC-update policy.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x12178-nppt_smac_disable.md`.

### 0x12250 nppt_smac_enable
Status: complete
Confidence: verified MAC bound, normal/RGMII/XMAC branches, config-bit set,
asymmetric SOPC update rules, void return, and sole caller.
Role: Enable a normal SMAC, RGMII MAC6, or delegated XMAC data path on link up.
Inputs/outputs: MAC byte; no meaningful return value.
Globals/MMIO/callbacks: Reads `g_smac_max_index`, updates NPPT/RGMII config and
SOPC state, or delegates XMAC TX/RX enable.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls shared state.
Evidence: Complete 66-instruction body and one direct `check_phy` caller xref.
Open questions: Equality-only normal SOPC update and config/SOPC bit meanings.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x12250-nppt_smac_enable.md`.

### 0x1295c nppt_smac_set_uni_mode
Status: complete
Confidence: verified offset arithmetic, field-clear mask, raw mode OR, void
return, and sole caller.
Role: Update a raw UNI-mode field for one selector-derived NPPT word.
Inputs/outputs: Unsigned MAC and raw mode values; no meaningful return value.
Globals/MMIO/callbacks: Volatile RMW at `nppt_base + 4 * ((mac + 5) &
0x3fffffff)`.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the RMW.
Evidence: Complete 9-instruction body and one direct `nppt_smac_init` caller xref.
Open questions: Hardware field semantics and valid mode value constraints.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1295c-nppt_smac_set_uni_mode.md`.

### 0x16bbc xmac_tx_rx_enable
Status: complete
Confidence: verified raw selector preservation, RX-then-TX ordering, void return,
and all three direct callers.
Role: Enable RX then TX for one XMAC selector.
Inputs/outputs: Raw unsigned selector; no meaningful return value.
Globals/MMIO/callbacks: No direct global/MMIO access; delegates to RX/TX enable
helpers.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 8-instruction body and three direct caller xrefs.
Open questions: Hardware behavior on partial RX/TX enable.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16bbc-xmac_tx_rx_enable.md`.

### 0x16bdc xmac_tx_rx_disable
Status: complete
Confidence: verified raw selector preservation, RX-then-TX ordering, void
return, and sole direct caller.
Role: Disable RX then TX for one raw XMAC selector.
Inputs/outputs: Raw unsigned selector; no meaningful return value.
Globals/MMIO/callbacks: No direct global/MMIO access; delegates to RX/TX disable
helpers.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 8-instruction body and one direct caller xref.
Open questions: Hardware behavior on partial RX/TX disable.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16bdc-xmac_tx_rx_disable.md`.

### 0x16a0c xmac_tx_disable
Status: complete
Confidence: verified raw selector tests/masks, special/NPPT-relative register
formulas, bit-zero clear RMW, void return, and all ten direct callers.
Role: Clear TX-enable bit zero for one raw XMAC selector.
Inputs/outputs: Raw unsigned selector; no meaningful return value.
Globals/MMIO/callbacks: Volatile RMW on a special or NPPT-relative TX word.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the RMW.
Evidence: Complete 25-instruction body and ten direct caller xrefs.
Open questions: Selector-width behavior and TX control bit meaning.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16a0c-xmac_tx_disable.md`.

### 0x16ad4 xmac_rx_disable
Status: complete
Confidence: verified raw selector tests/masks, special/NPPT-relative register
formulas, bit-zero clear RMW, void return, and all ten direct callers.
Role: Clear RX-enable bit zero for one raw XMAC selector.
Inputs/outputs: Raw unsigned selector; no meaningful return value.
Globals/MMIO/callbacks: Volatile RMW on a special or NPPT-relative RX word.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the RMW.
Evidence: Complete 29-instruction body and ten direct caller xrefs.
Open questions: Selector-width behavior and RX control bit meaning.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16ad4-xmac_rx_disable.md`.

### 0x16984 xmac_get_nppt_glb_link_status
Status: complete
Confidence: verified register offset, ARM variable-shift behavior, normalized
output, void return, and sole caller.
Role: Read a selected global NPPT link-status bit for an XMAC selector.
Inputs/outputs: Raw unsigned selector and output-word pointer; no meaningful
return value.
Globals/MMIO/callbacks: Volatile read at `nppt_base + 0x84`.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; output storage belongs to the caller.
Evidence: Complete 7-instruction body and one direct `phy_zxic051_check` caller
xref.
Open questions: Global-link word identity and bit-to-XMAC mapping.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16984-xmac_get_nppt_glb_link_status.md`.

### 0x16cdc xmac_get_uni_speed_from_xmac
Status: complete
Confidence: verified selector handling, speed-select field read, all computed
dispatch targets, output write, void return, and sole caller.
Role: Convert XMAC speed-select bits into the PHY-facing UNI speed code.
Inputs/outputs: XMAC byte and output-word pointer; no meaningful return value.
Globals/MMIO/callbacks: Reads a selector-specific speed-control word and uses an
internal eight-entry computed-dispatch table.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; output storage belongs to the caller.
Evidence: Complete 20-instruction dispatcher, all eight tail stubs, and one
direct `phy_zxic051_check` caller xref.
Open questions: Nonmonotonic speed code mapping and duplicate value four.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16cdc-xmac_get_uni_speed_from_xmac.md`.

### 0x16a70 xmac_tx_enable
Status: complete
Confidence: verified raw selector tests/masks, special/NPPT-relative register
formulas, bit-zero RMW, void return, and sole caller.
Role: Set TX-enable bit zero for one raw XMAC selector.
Inputs/outputs: Raw unsigned selector; no meaningful return value.
Globals/MMIO/callbacks: Volatile RMW on a special or NPPT-relative TX word.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the RMW.
Evidence: Complete 25-instruction body and one direct wrapper caller xref.
Open questions: Selector-width behavior and TX control bit meaning.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16a70-xmac_tx_enable.md`.

### 0x16b48 xmac_rx_enable
Status: complete
Confidence: verified raw selector tests/masks, special/NPPT-relative register
formulas, bit-zero RMW, void return, and sole caller.
Role: Set RX-enable bit zero for one raw XMAC selector.
Inputs/outputs: Raw unsigned selector; no meaningful return value.
Globals/MMIO/callbacks: Volatile RMW on a special or NPPT-relative RX word.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the RMW.
Evidence: Complete 29-instruction body and one direct wrapper caller xref.
Open questions: Selector-width behavior and RX control bit meaning.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x16b48-xmac_rx_enable.md`.

### 0x1874c xmac_set_pcs_for_sgmii_half_duplex
Status: complete
Confidence: verified mode/auto gates, full argument ABI, PCS-call order, AN
state branches, void return, and both direct caller sites.
Role: Reconcile PCS SGMII control state around the XMAC half-duplex PHY path.
Inputs/outputs: Raw unsigned XMAC selector, configure selector, speed word, and
state word; no meaningful return value. The raw selector indexes shared state
arrays before the PCS calls use its low byte.
Globals/MMIO/callbacks: Reads `sg_xmac_work_mode[xmac]` and
`g_xmac_work_in_auto[xmac]`; delegates all PCS register access.
Concurrency/ownership: No local synchronization or ownership transfer. It runs
from the `check_phy` polling path.
Evidence: Complete ARM64 body; the `MOV W2,W3` before the configure branch,
which establishes the three-argument PCS helper ABI; both `check_phy` xrefs;
and direct analysis of the AN-bit reader/setter and the speed/duplex helper.
IDA type at `0x1874c` was updated to the recovered four-`unsigned int` void
signature.
Open questions: Exact PCS meanings of the `state` word and the link-status
field; the name comes from the vendor symbol rather than independently observed
hardware terminology.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1874c-xmac_set_pcs_for_sgmii_half_duplex.md`.

### 0x19ba4 xpcs_set_speed_duplex_in_sgmii_anto_disale_mode
Status: complete
Confidence: verified selector narrowing, three argument ABI, all three PCS
calls, fixed link-status value, void return, and sole direct caller.
Role: Write one explicit SGMII PCS speed/duplex/link-status configuration.
Inputs/outputs: XMAC byte selector, raw speed word, raw state word; no
meaningful return value.
Globals/MMIO/callbacks: No direct global or MMIO expression in this wrapper;
delegates to three PCS register helpers.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer.
Evidence: Complete 14-instruction ARM64 body, preserved third input in `W7`,
three direct call sites, and the sole xref from `0x1874c`. IDA type at `0x19ba4`
was updated to the recovered three-argument void signature.
Open questions: Vendor spelling `anto_disale` and exact hardware name of the
state input remain unverified.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19ba4-xpcs_set_speed_duplex_in_sgmii_anto_disale_mode.md`.

### 0x18f44 xpcs_set_sr_mii_ctrl_speed
Status: complete
Confidence: verified selector narrowing/address branches, volatile-read order,
all six speed encodings, invalid-input no-write path, void return, and all six
direct callers.
Role: Replace the SR-MII control speed field for one PCS selector.
Inputs/outputs: XMAC byte selector and raw unsigned speed code; no meaningful
return value. Inputs outside one through six leave the register unwritten after
its initial volatile read.
Globals/MMIO/callbacks: Reads and conditionally writes a selector-specific PCS
control word; obtains the nonspecial window from `xmac0_pcs_base`.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the register RMW.
Evidence: Complete 50-instruction ARM64 body; computed jump-table targets;
`0xffffdf9f` mask; direct window and base-relative address paths; and six direct
caller xrefs. IDA type at `0x18f44` was updated to the recovered void signature.
Open questions: Hardware names for bits 13, 6, and 5; intended behavior for
selectors outside the observed XMAC range.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18f44-xpcs_set_sr_mii_ctrl_speed.md`.

### 0x19010 xpcs_set_sr_mii_ctrl_duplex_mode
Status: complete
Confidence: verified selector/address branches, byte and low-bit input use,
bit-eight RMW, void return, and all six direct callers.
Role: Replace bit eight of one PCS SR-MII control word from a raw state input.
Inputs/outputs: XMAC byte selector and raw unsigned state; only state bit zero
is written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS control word
through the same direct/base-relative addressing used by the speed writer.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, both pointer paths, input byte
truncation, bit-eight insertion, and six direct caller xrefs. IDA type at
`0x19010` was updated to the recovered void signature.
Open questions: Exact PCS meaning of bit eight and the original source type/name
of the raw state argument.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19010-xpcs_set_sr_mii_ctrl_duplex_mode.md`.

### 0x19104 xpcs_set_sr_mii_ctrl_an_enable
Status: complete
Confidence: verified selector/address branches, byte and low-bit input use,
bit-12 RMW, void return, and all nine direct callers.
Role: Replace bit 12 of one PCS SR-MII control word from a raw enable input.
Inputs/outputs: XMAC byte selector and raw unsigned enable; only enable bit zero
is written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS control word
through the same direct/base-relative addressing used by the adjacent writers.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, byte truncation, bit-12 insertion,
both pointer paths, and nine direct caller xrefs. IDA type at `0x19104` was
updated to the recovered void signature.
Open questions: Exact hardware name and behavior of SR-MII control bit 12.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19104-xpcs_set_sr_mii_ctrl_an_enable.md`.

### 0x19184 xpcs_sr_mii_ctrl_is_an_enable
Status: complete
Confidence: verified selector/address branches, bit-12 extraction, normalized
unsigned return, and sole direct caller.
Role: Read SR-MII control bit 12 for one PCS selector.
Inputs/outputs: XMAC byte selector; returns zero or one from control bit 12.
Globals/MMIO/callbacks: Volatile read of the selector-specific PCS control word
through the same direct/base-relative address logic as adjacent writers.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer.
Evidence: Complete 17-instruction ARM64 body, both address branches, `UBFX`
bit-12 extraction, and sole direct xref from `0x1874c`. IDA type at `0x19184`
was updated to the recovered byte-selector/unsigned-result signature.
Open questions: Exact PCS behavior associated with the vendor AN-enable name.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19184-xpcs_sr_mii_ctrl_is_an_enable.md`.

### 0x1964c xpcs_set_vr_mii_an_ctrl_sgmii_link_sts
Status: complete
Confidence: verified selector/address branches, byte and low-bit input use,
bit-four RMW, void return, and all five direct callers.
Role: Replace bit four of a PCS SGMII link-status control word.
Inputs/outputs: XMAC byte selector and raw unsigned link status; only status bit
zero is written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at the
verified `0x7e0004` offset through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 31-instruction ARM64 body, byte truncation, bit-four
insertion, both pointer paths, and five direct caller xrefs. IDA type at
`0x1964c` was updated to the recovered void signature.
Open questions: Exact PCS role of the link-status word and bit four.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1964c-xpcs_set_vr_mii_an_ctrl_sgmii_link_sts.md`.

### 0x196cc xpcs_auto_negotiation_conf_in_sgmii_mode
Status: complete
Confidence: verified two-argument ABI, selector/mode rejection, exact PCS call
order, flag-one branch, global write, status returns, and both direct callers.
Role: Configure SGMII PCS auto-negotiation state for one valid XMAC selector.
Inputs/outputs: XMAC byte and low-byte auto-enable flag; returns zero on a
mode-three configuration path, otherwise `-1`.
Globals/MMIO/callbacks: Reads `sg_xpcs_mode[xmac]`, writes
`g_xmac_work_in_auto[xmac]` only for flag one, and delegates PCS control writes.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer.
Evidence: Complete 42-instruction ARM64 body; saved low-byte `W1` input;
selector/mode gates; exact call order; two xrefs from `xmac_sgmii_conf`; and
corrected IDA type at `0x196cc`.
Open questions: Exact interrupt and MAC-auto-switch bit semantics, and whether
the missing global-flag clear on non-one input is intentional policy.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x196cc-xpcs_auto_negotiation_conf_in_sgmii_mode.md`.

### 0x192c8 xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw
Status: complete
Confidence: verified selector/address branches, byte input, bit-nine RMW, void
return, and all four direct callers.
Role: Replace bit nine of a PCS VR-MII digital-control word from a byte enable.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x7e0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-nine insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact hardware semantics of the vendor MAC-auto-switch bit.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x192c8-xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw.md`.

### 0x193c8 xpcs_set_vr_mii_an_ctrl_an_intr_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-zero RMW, void
return, and all five direct callers.
Role: Replace bit zero of a PCS VR-MII AN-control word from a byte enable.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x7e0004` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 31-instruction ARM64 body, exact bit-zero insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact physical semantics of the vendor AN-interrupt-enable bit.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x193c8-xpcs_set_vr_mii_an_ctrl_an_intr_en.md`.

### 0x19448 xpcs_auto_negotiation_conf_in_usxgmii_mode
Status: complete
Confidence: verified two-byte-argument ABI, selector rejection, PCS call order,
flag-one global update, status returns, and all seven direct caller sites.
Role: Configure USXGMII PCS auto-negotiation controls for one XMAC selector.
Inputs/outputs: XMAC byte and low-byte auto-enable flag; returns zero for valid
selectors and `-1` after logging for selectors above four.
Globals/MMIO/callbacks: Writes the shared `g_xmac_work_in_auto[xmac]` byte only
when the input byte equals one; delegates two PCS control writes.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer.
Evidence: Complete 25-instruction ARM64 body, both byte truncations, exact calls
and branch, seven direct xrefs, and corrected IDA type at `0x19448`.
Open questions: Why non-one odd flags can set PCS low bits while not updating
the shared auto-mode byte.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19448-xpcs_auto_negotiation_conf_in_usxgmii_mode.md`.

### 0x1952c xpcs_1g_mode_conf
Status: complete
Confidence: verified four-argument ABI, fixed call sequence, PSEQ result branch,
low-power behavior, PCS-mode write, status return, and both direct callers.
Role: Program a 1G PCS configuration sequence and await PSEQ completion.
Inputs/outputs: XMAC byte, raw speed word, raw duplex word, and raw PCS-mode
word; returns zero after a successful wait or `-1` after a nonzero wait result.
Globals/MMIO/callbacks: No direct global/MMIO expression; delegates all PCS
configuration and PSEQ polling to lower helpers.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer. On PSEQ failure it intentionally does not disable low-power.
Evidence: Complete 40-instruction ARM64 body, direct analysis of the PCS-type
writer, both wrapper call sites with fixed PCS modes zero/two, and corrected IDA
type at `0x1952c`.
Open questions: Exact fixed constants in the delegated constprop helpers and
hardware meaning of the PCS-mode word.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1952c-xpcs_1g_mode_conf.md`.

### 0x194b0 xpcs_set_vr_mii_an_ctrl_pcs_mode
Status: complete
Confidence: verified selector/address branches, two-bit input use, bits-one/two
RMW, void return, and both direct callers.
Role: Replace bits one and two of a PCS VR-MII AN-control word from a PCS-mode
input.
Inputs/outputs: XMAC byte selector and raw PCS-mode word; only PCS-mode bits
zero/one are written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x7e0004` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 30-instruction ARM64 body, exact two-bit insertion and mask,
updated to the recovered void `(u8, u32)` signature.
Open questions: Exact hardware decoding of the two PCS-mode bits.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x194b0-xpcs_set_vr_mii_an_ctrl_pcs_mode.md`.

### 0x19d5c xpcs_prepare_for_switch_mode
Status: complete
Confidence: verified two-argument ABI, auto-flag clear, comparison gate,
mode-dispatch cases, discarded exit returns, void return, and all seven callers.
Role: Clear PCS auto state and conditionally exit the cached PCS mode before a
new mode configuration.
Inputs/outputs: XMAC byte selector and raw target-mode word; no meaningful
return value.
Globals/MMIO/callbacks: Clears `g_xmac_work_in_auto[xmac]`, reads
`sg_xpcs_mode[xmac]`, and calls one of three PCS exit helpers.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer. It updates the auto-state byte without updating cached mode.
Evidence: Complete 27-instruction ARM64 body, jump-table dispatch, seven direct
caller xrefs, and corrected IDA type at `0x19d5c`.
Open questions: Why no exit is selected for cached mode four and why exit
statuses are ignored.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19d5c-xpcs_prepare_for_switch_mode.md`.

### 0x19cd4 xpcs_exit_sgmii_mode
Status: complete
Confidence: verified byte selector, fixed six-call clear sequence, void return,
and sole direct caller.
Role: Clear the PCS controls used by the SGMII mode before a mode transition.
Inputs/outputs: XMAC byte selector; no meaningful return value.
Globals/MMIO/callbacks: No direct global/MMIO expression; delegates six PCS
control clears.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer.
Evidence: Complete 23-instruction ARM64 body, six literal-zero call arguments,
sole xref from `xpcs_prepare_for_switch_mode`, and IDA type at `0x19cd4` set to
the recovered void byte signature.
Open questions: Exact hardware semantics of the two 2.5G-mode control flags.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19cd4-xpcs_exit_sgmii_mode.md`.

### 0x191c8 xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-two RMW, void
return, and all four direct callers.
Role: Replace bit two of a PCS VR-MII digital-control word from a byte enable.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x7e0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-two insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact physical role of the vendor MII 2.5G-mode control bit.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x191c8-xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en.md`.

### 0x19d30 xpcs_exit_usxgmii_mode
Status: complete
Confidence: verified byte selector, fixed two-call sequence, void return, and
sole direct caller.
Role: Clear USXGMII enable and restore VSMMD1 enable before a PCS mode change.
Inputs/outputs: XMAC byte selector; no meaningful return value.
Globals/MMIO/callbacks: No direct global/MMIO expression; delegates two PCS
control writes.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer.
Evidence: Complete 11-instruction ARM64 body, literal-zero/one call arguments,
sole xref from `xpcs_prepare_for_switch_mode`, and IDA type at `0x19d30` set to
the recovered void byte signature.
Open questions: Exact hardware relationship between USXGMII and VSMMD1 controls.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19d30-xpcs_exit_usxgmii_mode.md`.

### 0x19b60 xpcs_exit_hsgmii_mode
Status: complete
Confidence: verified byte selector, fixed four-call clear sequence, void return,
and sole direct caller.
Role: Clear the PCS controls used by HSGMII mode before a mode transition.
Inputs/outputs: XMAC byte selector; no meaningful return value.
Globals/MMIO/callbacks: No direct global/MMIO expression; delegates four PCS
control clears.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer.
Evidence: Complete 17-instruction ARM64 body, four literal-zero call arguments,
sole xref from `xpcs_prepare_for_switch_mode`, and IDA type at `0x19b60` set to
the recovered void byte signature.
Open questions: Exact hardware behavior of the paired MII and XS/PCS VSMMD1
controls during HSGMII transition.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19b60-xpcs_exit_hsgmii_mode.md`.

### 0x19348 xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-13 RMW, void
return, and both direct callers.
Role: Replace bit 13 of a PCS VR-MII digital-control word from a byte enable.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x7e0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-13 insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact physical role of the vendor VSMMD1 control bit.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19348-xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en.md`.

### 0x18ec4 xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-13 RMW, void
return, and all four direct callers.
Role: Replace bit 13 of a PCS VR-XS/PCS digital-control word from a byte enable.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x0e0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-13 insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact physical role of the vendor XS/PCS VSMMD1 control bit.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18ec4-xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en.md`.

### 0x18930 xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-nine RMW, void
return, and both direct callers.
Role: Replace bit nine of a PCS VR-XS/PCS digital-control word from a byte
USXGMII-enable input.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x0e0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-nine insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact physical role of the vendor USXGMII-enable bit.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18930-xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en.md`.

### 0x1a210 xpcs_usxgmii_mode_conf
Status: complete
Confidence: verified two-argument ABI, selector gate, fixed transition target,
PCS call/RMW/reset order, post-write mode validation, cached-mode mapping,
status returns, and all six direct callers.
Role: Configure a USXGMII PCS mode and update its cached mode classification.
Inputs/outputs: XMAC byte selector and raw USXGMII mode word; returns zero for
accepted mode codes or `-1` after an invalid selector/mode log.
Globals/MMIO/callbacks: Writes `sg_xpcs_mode[xmac]` only after accepted mode
codes; RMWs the selector-specific PCS word at offset `0x0e001c`; invokes the
mode-transition, PCS enable, reset, and wait helpers.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer. The reset wait result is ignored, and invalid mode inputs
have already changed PCS state before returning `-1`.
Evidence: Complete 85-instruction ARM64 body, all selector/address branches,
three-bit field insertion, reset/wait call order, complete mode mapping, six
direct caller xrefs, and corrected IDA type at `0x1a210`.
Open questions: Exact physical meaning of the mode-field encodings and why
transition preparation uses fixed target mode five for all accepted inputs.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1a210-xpcs_usxgmii_mode_conf.md`.

### 0x189b0 xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst
Status: complete
Confidence: verified selector/address branches, byte input, bit-15 RMW, void
return, and sole direct caller.
Role: Replace bit 15 of a PCS VR-XS/PCS digital-control word from a byte reset
input.
Inputs/outputs: XMAC byte selector and reset byte; only reset bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x0e0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-15 insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact reset timing and hardware behavior associated with bit 15.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x189b0-xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst.md`.

### 0x18b3c xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared
Status: complete
Confidence: verified selector/address branches, reset-bit poll, exact retry and
delay behavior, status return, and all three direct callers.
Role: Poll PCS VR reset bit 15 until clear or a fixed retry budget is exhausted.
Inputs/outputs: XMAC byte selector; returns zero after observing reset clear or
`-1` after 400 set-bit observations and delays.
Globals/MMIO/callbacks: Repeated volatile reads of the selector-specific PCS
word at offset `0x0e0000`; calls `__const_udelay(859000)` after each set read.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer. The PCS base pointer is reloaded through the address helper each poll.
Evidence: Complete 36-instruction ARM64 body, 400-counter, bit-15 test, exact
delay argument, three direct caller xrefs, and corrected IDA type at `0x18b3c`.
Open questions: Exact time unit represented by `__const_udelay(859000)` and
whether reset bit 15 is hardware self-clearing on every PCS variant.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18b3c-xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared.md`.

### 0x18e44 xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-10 RMW, void
return, and both direct callers.
Role: Replace bit 10 of a PCS VR-XS/PCS digital-control word from a byte reset
enable input.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x0e0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-10 insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact physical role and reset relationship of the USRA control.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18e44-xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en.md`.

### 0x19090 xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode
Status: complete
Confidence: verified three-argument ABI, selector gate, speed/duplex call order,
delay/reset/wait sequence, status return, and no direct code xrefs.
Role: Apply explicit USXGMII speed/duplex settings while auto-negotiation is
disabled, then wait for reset completion.
Inputs/outputs: XMAC byte selector, raw speed word, raw duplex word; returns
zero or `-1` from the VR-reset wait, or `-1` after an invalid-selector log.
Globals/MMIO/callbacks: No direct global/MMIO expression; delegates PCS speed,
duplex, USRA reset, and VR-reset wait operations.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer.
Evidence: Complete 28-instruction ARM64 body, saved third input in `W6`, exact
delay/reset/wait sequence, and no direct code xrefs. IDA type at `0x19090` was
updated to the recovered three-argument `int` signature.
Open questions: Why this function has no direct code xrefs in the current IDB
and whether it is reached indirectly or retained as unused vendor support code.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19090-xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode.md`.

### 0x19910 xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode
Status: complete
Confidence: verified four-argument ABI, status snapshot branches, output-write
order, speed/duplex decoding, PCS/reset sequence, wait status return, and all
three direct callers.
Role: Handle a completed USXGMII auto-negotiation interrupt and apply its
reported speed/duplex settings.
Inputs/outputs: XMAC byte selector and output pointers for UNI speed, duplex,
and auto status; returns zero, `-1`, or VR-reset wait status as described by the
captured AN status snapshot.
Globals/MMIO/callbacks: Volatile read of selector-specific AN status at
`0x7e0008`; clears completion state and delegates speed mapping/PCS/reset work.
Concurrency/ownership: No local synchronization or allocation. Output pointers
are caller-owned; on the bit-14 path they are committed before reset wait ends.
Evidence: Complete 79-instruction ARM64 body, all snapshot-bit branches, exact
output stores, three direct caller xrefs, and corrected IDA type at `0x19910`.
Open questions: Exact meanings of AN status bits zero/14 and the lower speed
mapping's hardware codebook.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19910-xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode.md`.

### 0x19840 xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta
Status: complete
Confidence: verified selector/address branches, bit-zero RMW, void return, and
both direct callers.
Role: Clear bit zero of one PCS VR-MII AN interrupt-status word.
Inputs/outputs: XMAC byte selector; no meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x7e0008` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 28-instruction ARM64 body, exact bit-zero mask, both pointer
paths, and two direct caller xrefs. IDA type at `0x19840` was updated to the
recovered void byte signature.
Open questions: Exact AN event represented by status bit zero.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19840-xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta.md`.

### 0x198b4 xpcs_switch_vr_mii_an_intr_sts_speed
Status: complete
Confidence: verified all computed-dispatch targets, output write, unsigned
return, and both direct callers.
Role: Convert one raw VR-MII AN status speed code to a UNI speed code.
Inputs/outputs: Raw unsigned speed code and output-word pointer; writes and
returns the converted unsigned code.
Globals/MMIO/callbacks: No direct global, MMIO, or callback access.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer; output storage belongs to the caller.
Evidence: Complete 22-instruction dispatch, six explicit targets plus default,
Open questions: Exact physical speed meanings of the raw and UNI codebooks.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x198b4-xpcs_switch_vr_mii_an_intr_sts_speed.md`.

### 0x19bdc xpcs_get_speed_duplex_in_auto_en_sgmii_mode
Status: complete
Confidence: verified four-argument ABI, status-snapshot branches, output-write
order, speed/duplex decoding, status return, and both direct callers.
Role: Read a completed SGMII auto-negotiation result into caller-owned outputs.
Inputs/outputs: XMAC byte selector and output pointers for UNI speed, duplex,
and auto status; returns zero for a bit-zero completion branch or `-1` for a
missing completion/invalid selector.
Globals/MMIO/callbacks: Volatile AN status read at `0x7e0008`; clears completion
state and delegates raw-speed conversion.
Concurrency/ownership: No local synchronization or allocation. Output pointers
are caller-owned and only speed/duplex outputs are written in the bit-four path.
Evidence: Complete 61-instruction ARM64 body, snapshot bit branches, exact
output stores, two direct caller xrefs, and corrected IDA type at `0x19bdc`.
Open questions: Exact PCS semantics of AN status bits zero/four and raw fields.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19bdc-xpcs_get_speed_duplex_in_auto_en_sgmii_mode.md`.

### 0x1a370 xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode
Status: complete
Confidence: verified byte selector, local output initialization, retry/delay
behavior, status return, timeout log, and no direct code xrefs.
Role: Wait for the USXGMII auto-enable completion handler to return success.
Inputs/outputs: XMAC byte selector; returns the successful zero result or the
last nonzero handler status after retry exhaustion.
Globals/MMIO/callbacks: No direct MMIO/global expression; delegates all PCS
status handling to `xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode`.
Concurrency/ownership: No local synchronization, allocation, cleanup, or
ownership transfer. Its local output words are discarded after every attempt.
Evidence: Complete 43-instruction ARM64 body, 400-counter, exact delay/log
path, no direct code xrefs, and corrected IDA type at `0x1a370`.
Open questions: Why the wrapper is retained despite no direct code xrefs, and
whether its local output writes are only a required side effect of the callee.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x1a370-xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode.md`.

### 0x18870 xpcs_set_sr_xs_pcs_ctrl1_low_power_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-11 RMW, void
return, and all eight direct callers.
Role: Replace bit 11 of one PCS SR-XS/PCS control word from a byte low-power
enable input.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x0c0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-11 insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact physical role of the SR-XS/PCS low-power bit.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18870-xpcs_set_sr_xs_pcs_ctrl1_low_power_en.md`.

### 0x188f0 xpcs_set_sr_xs_pcs_ctrl2_pcs_type
Status: complete
Confidence: verified selector/address branches, full-word direct store, void
return, and all six direct callers.
Role: Replace a PCS SR-XS/PCS type word for one selector.
Inputs/outputs: XMAC byte selector and raw PCS type word; no meaningful return.
Globals/MMIO/callbacks: Direct volatile store to selector-specific PCS offset
`0x0c001c` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; writer synchronization is caller-owned.
Evidence: Complete 15-instruction ARM64 body, both pointer paths, direct store
without read/mask, six direct caller xrefs, and corrected IDA type at `0x188f0`.
Open questions: Exact hardware decoding of the raw type word.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x188f0-xpcs_set_sr_xs_pcs_ctrl2_pcs_type.md`.

### 0x18c68 xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode.constprop.1
Status: complete
Confidence: verified selector/address branches, bit-zero RMW, void return, and
both direct callers.
Role: Clear bit zero of one PCS VR-XS/PCS XAUI-control word.
Inputs/outputs: XMAC byte selector; no meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x0e0010` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 28-instruction ARM64 body, exact bit-zero mask, both pointer
paths, and two direct caller xrefs. IDA type at `0x18c68` was updated to the
recovered void byte signature.
Open questions: Exact XAUI-control interpretation of bit zero.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18c68-xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode-constprop-1.md`.

### 0x18cdc xpcs_set_sr_xs_pcs_ctrl1_speed_sel.constprop.2
Status: complete
Confidence: verified selector/address branches, bit-13 RMW, void return, and
both direct callers.
Role: Clear bit 13 of the PCS SR-XS/PCS control1 word at offset `0x0c0000`.
Evidence: Complete 29-instruction ARM64 body, `0xffffdfff` mask, two pointer
paths, two caller xrefs, and recovered void byte IDA type.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18cdc-xpcs_set_sr_xs_pcs_ctrl1_speed_sel-constprop-2.md`.

### 0x18d50 xpcs_set_sr_pma_ctrl_speed_sel.constprop.3
Status: complete
Confidence: verified selector/address branches, bit-13 RMW, void return, and
all three direct callers.
Role: Clear bit 13 of the PCS SR-PMA control word at offset `0x040000`.
Evidence: Complete 29-instruction ARM64 body, `0xffffdfff` mask, two pointer
paths, three caller xrefs, and recovered void byte IDA type.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18d50-xpcs_set_sr_pma_ctrl_speed_sel-constprop-3.md`.

### 0x18a30 xpcs_eee_cfg
Status: complete
Confidence: verified two-byte-argument ABI, selector gate, three direct writes,
profile branch, void return, and no direct code xrefs.
Role: Program three fixed PCS EEE configuration words.
Inputs/outputs: XMAC byte selector and profile byte; no meaningful return value.
Globals/MMIO/callbacks: Direct volatile stores at offsets `0x0e0018`,
`0x0e0020`, and `0x0e0024` through selector-specific address paths.
Evidence: Complete 65-instruction ARM64 body, all fixed constants/profile branch,
zero direct code xrefs, and corrected IDA type at `0x18a30`.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18a30-xpcs_eee_cfg.md`.

### 0x19ee0 xpcs_2p5gbase_x_conf
Status: complete
Confidence: verified selector gate, fixed PCS sequence, PMA low-power pulse,
cached-mode write, status return, and both direct callers.
Role: Configure one PCS selector for 2.5GBASE-X mode.
Inputs/outputs: XMAC byte selector; returns zero or `-1` after an invalid-selector log.
Globals/MMIO/callbacks: Writes `sg_xpcs_mode[xmac] = 4`; delegates PCS type,
speed-select, and PMA low-power register operations.
Evidence: Complete 36-instruction ARM64 body, fixed constants, two caller xrefs,
and recovered `int (u8)` IDA type.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x19ee0-xpcs_2p5gbase_x_conf.md`.

### 0x187f0 xpcs_set_sr_pma_ctrl1_low_power_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-11 RMW, void
return, and both direct callers.
Role: Replace bit 11 of one PCS SR-PMA control word from a byte low-power input.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes selector-specific PCS offset `0x040000`.
Evidence: Complete 32-instruction ARM64 body, exact mask, two pointer paths,
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x187f0-xpcs_set_sr_pma_ctrl1_low_power_en.md`.

### 0x19dcc xpcs_10gbase_r_conf
Status: complete. Recovered 10GBASE-R PCS configuration transaction; selector
gate, fixed mode/type/low-power sequence, cached-mode write, return values, and
two direct callers verified. Record:
`functions/0x19dcc-xpcs_10gbase_r_conf.md`.

### 0x19e54 xpcs_5gbase_r_conf
Status: complete. Valid selectors prepare/cache mode one, write PCS type `0x0f`,
and pulse SR-XS/PCS low power around delay `859000`; invalid selectors log and
return `-1`. Two `xmac_5gbase_r_conf` caller sites verified. Record:
`functions/0x19e54-xpcs_5gbase_r_conf.md`.

### 0x19fe8 xpcs_hsgmii_mode_conf
Status: complete. Verified 108-instruction HSGMII transaction: selector gate,
CPU 129/133 transition/AN branches, fixed PCS setup, PSEQ failure return,
timer writes, cached mode eight, and two direct callers. Record:
`functions/0x19fe8-xpcs_hsgmii_mode_conf.md`.

### 0x19a50 xpcs_set_vr_mii_link_timer_ctrl
Status: complete. Direct raw timer store at selector-specific offset `0x7e0028`;
two callers and recovered void `(u8, u32)` type verified. Record:
`functions/0x19a50-xpcs_set_vr_mii_link_timer_ctrl.md`.

### 0x19a90 xpcs_auto_negotiation_conf_in_1000base_x_mode
Status: complete. Verified three-byte ABI, selector/mode gates, exact AN and
2.5G control branches, zero/-1 returns, and no direct code xrefs. Record:
`functions/0x19a90-xpcs_auto_negotiation_conf_in_1000base_x_mode.md`.

### 0x19f74 xpcs_1000base_x_conf
Status: complete. Verified `(u8 xmac, u32 speed, u32 duplex)` ABI, selector
gate, mode-two preparation/cache, 1G PCS sequence call, status return, and two
direct callers. Record: `functions/0x19f74-xpcs_1000base_x_conf.md`.

### 0x19248 xpcs_set_sr_mii_dig_ctrl1_cl37_tmr_ovr_ride
Status: complete. Byte-enable bit-3 RMW at offset `0x7e0000`; two callers and
recovered void byte signature verified. Record:
`functions/0x19248-xpcs_set_sr_mii_dig_ctrl1_cl37_tmr_ovr_ride.md`.

### 0x18dc4 xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en
Status: complete
Confidence: verified selector/address branches, byte input, bit-two RMW, void
return, and all four direct callers.
Role: Replace bit two of a PCS VR-XS/PCS digital-control word from a byte enable.
Inputs/outputs: XMAC byte selector and enable byte; only enable bit zero is
written. No meaningful return value.
Globals/MMIO/callbacks: Reads and writes the selector-specific PCS word at
offset `0x0e0000` through direct/base-relative addressing.
Concurrency/ownership: No local lock, allocation, cleanup, or ownership
transfer; caller serialization controls the volatile RMW.
Evidence: Complete 32-instruction ARM64 body, exact bit-two insertion and mask,
updated to the recovered void byte signature.
Open questions: Exact physical role of the vendor XS/PCS 2.5G-mode control bit.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x18dc4-xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en.md`.

### 0x195cc xpcs_set_vr_mii_an_ctrl_tx_config
Status: complete. Replaces bit three of selector-specific PCS offset `0x7e0004`
with the low bit of a byte enable argument. The sole direct caller is
`xpcs_init`; recovered void `(u8, u8)` type and both address paths verified.
Record: `functions/0x195cc-xpcs_set_vr_mii_an_ctrl_tx_config.md`.

### 0x19778 xpcs_set_vr_mii_an_ctrl_mii_ctrl
Status: complete. Replaces bit eight of selector-specific PCS offset `0x7e0004`
with the low bit of a byte enable argument. The sole direct caller is
`xpcs_init`; recovered void `(u8, u8)` type and both address paths verified.
Record: `functions/0x19778-xpcs_set_vr_mii_an_ctrl_mii_ctrl.md`.

### 0x197f8 xpcs_get_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta
Status: complete. Reads normalized bit zero from selector-specific PCS offset
`0x7e0008`, stores it through an unchecked output pointer, and returns it. No
direct code xrefs; recovered `(u8, u32 *) -> u32` type and both address paths
verified. Record: `functions/0x197f8-xpcs_get_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta.md`.

### 0x18bcc xpcs_wait_vr_xs_pcs_dig_sts_pseq_state.constprop.0
Status: complete. Polls selector-specific PCS offset `0x0e0040` until bits 2-4
are not value four, with 400 retries and `__const_udelay(859000)` between busy
polls. Two configuration callers and zero/-1 returns verified. Record:
`functions/0x18bcc-xpcs_wait_vr_xs_pcs_dig_sts_pseq_state.constprop.0.md`.

### 0x1a19c xpcs_sgmii_mode_conf
Status: complete. Valid selectors prepare target mode three, invoke
`xpcs_1g_mode_conf(xmac, mode_value, config_value, 2)`, cache mode three
regardless of that call's result, and return it. Invalid selectors log and
return `-1`; both callers and `(u8, u32, u32) -> int` ABI verified. Record:
`functions/0x1a19c-xpcs_sgmii_mode_conf.md`.

### 0x1a420 xpcs_init
Status: complete. Validates selectors zero through four, waits for reset bit 15
to clear in both SR-XS/PCS and SR-MII control words with independent 400-retry
loops, then clears two VR-MII AN-control bits. The sole caller ignores its
integer status. Record: `functions/0x1a420-xpcs_init.md`.

### 0x1a550 byPassEnableSet
Status: complete. On CPU 133 or 129 only, writes PCS offset `0x0e0014` bit four
according to whether a byte enable is nonzero; all other CPU types return zero
without MMIO. Nine XMAC mode callers and `(u8, u8) -> int` ABI verified. Record:
`functions/0x1a550-byPassEnableSet.md`.

### 0x1a608 phy_zx5201_check
Status: complete. Reads PHY extended register 26 twice, gates on bit six, maps
bits 8-9 to low status codes, and packs bit seven into bit 10 of its integer
return. No direct code xrefs; the `(u8) -> int` ABI is verified. Record:
`functions/0x1a608-phy_zx5201_check.md`.

### 0x1a688 phy_zx5201_init
Status: complete. Executes the fixed primary/adjacent PHY extended-MDIO script,
including two reads and a register-21 RMW, with nine `429500` delays; it returns
the final `printk` status. No direct code xrefs; `(u8) -> int` ABI verified.
Record: `functions/0x1a688-phy_zx5201_init.md`.

### 0x1a818 phy_8574_check
Status: complete. Gates on PHY register-one bit two, saves/selects/restores page
register 31 around two register-28 reads, then maps status bits 3-5 into a
packed integer result. No direct code xrefs; `(u8) -> int` ABI verified. Record:
`functions/0x1a818-phy_8574_check.md`.

### 0x1a8f0 phy_8574_init
Status: complete. Runs a once-only guarded register-18 poll, then a fixed
extended-MDIO page/register script with 13 delays and three RMWs; timeout only
logs and still sets the guard. No direct code xrefs; `(u8) -> int` ABI verified.
Record: `functions/0x1a8f0-phy_8574_init.md`.

### 0x1abb0 zte_gephy_set_eee_en
Status: complete. Selects MDIO register state through register 16, then replaces
register-17 bits one/two only when a byte enable equals one; all other values
clear them. No direct code xrefs; `(u8, u8) -> int` ABI verified. Record:
`functions/0x1abb0-zte_gephy_set_eee_en.md`.

### 0x1ac10 zte_gephy_set_energy_detect_power_down_en
Status: complete. Clears MDIO register-21 bit three and ORs the full byte enable
shifted left by three, without masking to a binary value. No direct code xrefs;
`(u8, u8) -> int` ABI verified. Record:
`functions/0x1ac10-zte_gephy_set_energy_detect_power_down_en.md`.

### 0x1ac58 zte_gephy_set_link_status_change_en
Status: complete. Replaces MDIO register-24 bit two from the low bit of a
halfword enable argument. No direct code xrefs; `(u8, u16) -> int` ABI verified.
Record: `functions/0x1ac58-zte_gephy_set_link_status_change_en.md`.

### 0x1aca4 zte_gephy_get_eee_en_status
Status: complete. Rejects null output pointers, otherwise selects MDIO register
state and writes register-17 bits one/two to an output byte. No direct code
xrefs; `(u8, u8 *) -> int` ABI verified. Record:
`functions/0x1aca4-zte_gephy_get_eee_en_status.md`.

### 0x1ad04 zte_gephy_get_short_reach_en
Status: complete. Rejects null output, otherwise reads APB-base slot offset
`0x90` bit zero when mapped; an empty slot returns zero without modifying the
output. No direct code xrefs; `(u8, u8 *) -> int` ABI verified. Record:
`functions/0x1ad04-zte_gephy_get_short_reach_en.md`.

### 0x1ad58 zte_gephy_get_energy_detect_power_down_en
Status: complete. Rejects null output, otherwise discards an MDIO register-21
read and stores zero to the output byte. No direct code xrefs; `(u8, u8 *) ->
int` ABI verified. Record:
`functions/0x1ad58-zte_gephy_get_energy_detect_power_down_en.md`.

### 0x1ada0 zte_gephy_get_1000m_tx_dac_lv
Status: complete. Rejects null output, otherwise selects MDIO register state and
stores register-17 low six bits through a halfword pointer. No direct code
xrefs; `(u8, u16 *) -> int` ABI verified. Record:
`functions/0x1ada0-zte_gephy_get_1000m_tx_dac_lv.md`.

### 0x1ae00 zte_gephy_get_1000m_tx_dac_slew
Status: complete. Rejects null output, otherwise selects MDIO register state and
stores register-17 low three bits through a halfword pointer. No direct code
xrefs; `(u8, u16 *) -> int` ABI verified. Record:
`functions/0x1ae00-zte_gephy_get_1000m_tx_dac_slew.md`.

### 0x1ae60 zte_gephy_get_100m_tx_dac_lv
Status: complete. Rejects null output, otherwise selects MDIO register state and
stores register-17 low six bits through a halfword pointer. No direct code
xrefs; `(u8, u16 *) -> int` ABI verified. Record:
`functions/0x1ae60-zte_gephy_get_100m_tx_dac_lv.md`.

### 0x1aec0 zte_gephy_get_100m_tx_dac_slew
Status: complete. Rejects null output, otherwise selects MDIO register state and
stores register-17 low three bits through a halfword pointer. No direct code
xrefs; `(u8, u16 *) -> int` ABI verified. Record:
`functions/0x1aec0-zte_gephy_get_100m_tx_dac_slew.md`.

### 0x1af20 zte_gephy_get_link_status_change_en
Status: complete. Rejects null output, otherwise reads MDIO register-24 bit two
into a halfword output. No direct code xrefs; `(u8, u16 *) -> int` ABI verified.
Record: `functions/0x1af20-zte_gephy_get_link_status_change_en.md`.

### 0x1af6c zte_gephy_get_link_status_change_event
Status: complete. Rejects null output, otherwise reads MDIO register-25 bit two
into a halfword output. No direct code xrefs; `(u8, u16 *) -> int` ABI verified.
Record: `functions/0x1af6c-zte_gephy_get_link_status_change_event.md`.

### 0x1afb8 zte_gephy_get_rx_stats
Status: complete. Logs three 16-bit MDIO counters selected through two
register-16 writes and returns the final logging status. No direct code xrefs;
`(u8) -> int` ABI verified. Record: `functions/0x1afb8-zte_gephy_get_rx_stats.md`.

### 0x1b048 zte_gephy_set_short_reach_en
Status: complete. Writes APB-base offset `0x90` through `apb_bit_write` only
when the byte-indexed base slot exists; empty slots return zero. No direct code
xrefs; `(u8, u8) -> int` ABI verified. Record:
`functions/0x1b048-zte_gephy_set_short_reach_en.md`.

### 0x1b08c zte_gephy_set_ref_clk_25M
Status: complete. Returns the APB address pointer after setting global GEPHY APB
offset `0x200018` bit 12 from input bit zero. No direct code xrefs. Record:
`functions/0x1b08c-zte_gephy_set_ref_clk_25M.md`.

### 0x1b0bc zte_gephy_set_100m_tx_dac_lv
Status: complete. Validates a halfword DAC level is at most 63, then selects
MDIO register state and replaces register-17 low six bits. No direct code xrefs;
`(u8, u16) -> int` ABI verified. Record:
`functions/0x1b0bc-zte_gephy_set_100m_tx_dac_lv.md`.

### 0x1b130 zte_gephy_set_1000m_tx_dac_slew
Status: complete. Validates a halfword DAC slew is at most seven, then selects
MDIO register state and replaces register-17 low three bits. No direct code
xrefs; `(u8, u16) -> int` ABI verified. Record:
`functions/0x1b130-zte_gephy_set_1000m_tx_dac_slew.md`.

### 0x1b1a4 zte_gephy_set_100m_tx_dac_slew
Status: complete. Validates a halfword DAC slew is at most seven, then selects
MDIO register state and replaces register-17 low three bits. No direct code
xrefs; `(u8, u16) -> int` ABI verified. Record:
`functions/0x1b1a4-zte_gephy_set_100m_tx_dac_slew.md`.

### 0x1b218 zte_gephy_set_1000m_tx_dac_lv
Status: complete. Validates a halfword DAC level is at most 63, then selects
MDIO register state and replaces register-17 low six bits. No direct code xrefs;
`(u8, u16) -> int` ABI verified. Record:
`functions/0x1b218-zte_gephy_set_1000m_tx_dac_lv.md`.

### 0x1b28c check_phy_gephy
Status: complete. Saves/restores MDIO register 30 around a delayed double read
of register 26, then maps status bits into the GEPHY probe result. Referenced by
the SMAC initialization callback table; `(u8) -> int` ABI verified. Record:
`functions/0x1b28c-check_phy_gephy.md`.

### 0x1b340 phy_zxicge_init
Status: complete. One-time initializer with an unused byte callback argument
that derives four adjacent GEPHY APB-base slots from the global base and returns the byte guard value. It
is referenced by the SMAC initialization callback table. Record:
`functions/0x1b340-phy_zxicge_init.md`.

### 0x1b3a0 zte_set_gephy_enable
Status: complete. Only an enable byte equal to one clears MDIO register-zero bit
11; every other value sets it. The SMAC initialization callback table references
the helper. Record: `functions/0x1b3a0-zte_set_gephy_enable.md`.

### 0x1b3f4 zte_get_gephy_enable
Status: complete. Reads MDIO register-zero bit 11 as active-low enable state and
writes an unchecked byte output. The SMAC initialization callback table
references it. Record: `functions/0x1b3f4-zte_get_gephy_enable.md`.

### 0x1b430 phy_zxic051_get_linkstate
Status: complete. Copies link, duplex, and converted speed from three contiguous
PHY-state array regions to unchecked byte outputs. Referenced by the XMAC ZXIC
PHY callback table. Record: `functions/0x1b430-phy_zxic051_get_linkstate.md`.

### 0x1b474 phy_zxic051_set_enable
Status: complete. Stores a byte enable as a u32 state-array word at `phy + 12`,
then calls a no-argument external notifier. Referenced by the XMAC ZXIC PHY
callback table. Record: `functions/0x1b474-phy_zxic051_set_enable.md`.

### 0x1b4a4 phy_zxic051_get_enable
Status: complete. No-argument wrapper that returns the direct result of an
external getter. Referenced by the XMAC ZXIC PHY callback table. Record:
`functions/0x1b4a4-phy_zxic051_get_enable.md`.

### 0x1b4b8 phy_zxic051_set_linkmode
Status: complete. PHY-ID-gated, indirect-MDIO mode transaction with force-mode,
speed, and duplex branches; invalid unforced combinations return zero without
writes. Referenced by the XMAC ZXIC PHY callback table. Record:
`functions/0x1b4b8-phy_zxic051_set_linkmode.md`.

### 0x1b794 phy_zxic051_set_loopback
Status: complete. PHY-ID-gated indirect-MDIO loopback transaction with distinct
enable and non-one scripts, including fixed RMW masks and delays. Referenced by
the XMAC ZXIC PHY callback table. Record:
`functions/0x1b794-phy_zxic051_set_loopback.md`.

### 0x1b9e0 phy_zxic051_get_loopback
Status: complete. PHY-ID-gated indirect GE-MDIO getter that writes register-zero
bit 14 through an unchecked byte output. Referenced by the XMAC ZXIC PHY
callback table. Record: `functions/0x1b9e0-phy_zxic051_get_loopback.md`.

### 0x1ba78 phy_zxic051_get_linkmode
Status: complete. No-argument wrapper that returns the direct result of an
external linkmode getter. Referenced by the XMAC ZXIC PHY callback table. Record:
`functions/0x1ba78-phy_zxic051_get_linkmode.md`.

### 0x1ba8c phy_zxic051_init_check
Status: complete. Reads two extended register-31 selectors through a signed
reader slot; the exact 0/1 result combination invokes external PHY init and
returns `-1`. Its sole caller is `phy_zxic051_para_init`. Record:
`functions/0x1ba8c-phy_zxic051_init_check.md`.

### 0x1bb1c phy_zxic051_para_init
Status: complete. Discards a first PHY-ID lookup then returns
`phy_zxic051_init_check(phy)`. Its sole caller is `xmac_zxic_phy_init`. Record:
`functions/0x1bb1c-phy_zxic051_para_init.md`.

### 0x1bb48 phy_zxic051_port_exist
Status: complete. Returns one only for byte port five when external port-use
state is nonzero; it still calls that helper for all inputs. Its sole caller is
`xmac_zxic_phy_init`. Record: `functions/0x1bb48-phy_zxic051_port_exist.md`.

### 0x1bb78 phy_zxic_051_phy_uni_check
Status: complete. Core external-PHY link state machine: initializes four outputs,
validates port/PHY state, applies NBASEx gating, and performs link-up/down MDIO,
APB, counter, and cached-state transitions. Its sole caller is
`phy_zxic051_check`. Record: `functions/0x1bb78-phy_zxic_051_phy_uni_check.md`.

### 0x1c3e8 plat_cleanupModule
Status: complete. Void module-exit wrapper that calls `nppt_exit` then
`pon_driver_unregister`. Record:
`functions/0x1c3e8-plat_cleanupModule.md`.

### 0x1c400 dg_timer_init
Status: complete. Reinitializes the shared timer, sets its expires word to
`jiffies + 500`, and returns `add_timer` status. Two `zx_pon_int` call sites
schedule it. Record: `functions/0x1c400-dg_timer_init.md`.

### 0x1c458 __fswab64
Status: complete. Pure 64-bit byte reversal implemented by ARM64 `REV`; called
twice by `dump_net_condition_set`. Record: `functions/0x1c458-fswab64.md`.

### 0x1c460 _idm_rx_refill
Status: complete. Allocates an IDM buffer, encodes the physical address at
`uBP_BUFFER_OFFSET + 64` through a 32-bit byte swap, and posts it to an unchecked
descriptor word. Two `idm_init` call sites use it. Record:
`functions/0x1c460-idm_rx_refill.md`.

### 0x00000 getEponDeactiveState
Status: complete. Returns the full 32-bit `g_epon_deactive` global without
synchronization. No direct code xrefs; recovered `() -> u32` ABI. Record:
`functions/0x00000-getEponDeactiveState.md`.

### 0x0000c setEponDeactiveState
Status: complete. Normalizes an integer to the 32-bit deactivation global and
returns its incidental materialized address. No direct code xrefs; recovered
`(int) -> u32 *` ABI. Record: `functions/0x0000c-setEponDeactiveState.md`.

### 0x00020 pon_set_8k_out_en
Status: complete. Conditionally updates pin-mux bits 1-2 with equality-one input
semantics. Its sole caller is the PON PLL reference selector. Record:
`functions/0x00020-pon_set_8k_out_en.md`.

### 0x00060 pon_set_1pps_out_en
Status: complete. Conditionally updates `pin_mux_base + 8` bits 21-23 with
equality-one input semantics. Its sole caller is PON ToD output control. Record:
`functions/0x00060-pon_set_1pps_out_en.md`.

### 0x000a0 pon_set_uart1_txd_en
Status: complete. Conditionally updates base pin-mux bits 27-28 with
equality-one input semantics. Its sole caller is PON ToD output control. Record:
`functions/0x000a0-pon_set_uart1_txd_en.md`.

### 0x000dc pon_set_1pps_tod_out_en
Status: complete. Forwards one byte enable to the ordered 1PPS and UART1 pin-mux
writers, ignores their returns, and returns zero. No direct code xrefs. Record:
`functions/0x000dc-pon_set_1pps_tod_out_en.md`.

### 0x00104 pon_set_pin_mux_13
Status: complete. Replaces base pin-mux bits 25-26 from the low two input bits.
Its sole caller is the PON PLL reference selector. Record:
`functions/0x00104-pon_set_pin_mux_13.md`.

### 0x001dc isCpuType_133
Status: complete. Returns normalized equality of `g_pon_cputype` with literal
two. It has 67 direct callers across PON, SerDes, IDM, and XMAC/PCS paths.
Record: `functions/0x001dc-isCpuType_133.md`.

### 0x001f0 isCpuType_132
Status: complete. Returns normalized equality of `g_pon_cputype` with literal
one. It has 30 direct callers across PON, SerDes, IDM, and XMAC paths. Record:
`functions/0x001f0-isCpuType_132.md`.

### 0x00204 isCpuType_129
Status: complete. Returns normalized equality of `g_pon_cputype` with literal
four. It has 53 direct callers across PON, SerDes, IDM, and XMAC/PCS paths.
Record: `functions/0x00204-isCpuType_129.md`.

### 0x00218 ponserdes_to_xmac1_en_set
Status: complete. Routes PON SerDes with inverse PON/NPPT hardware controls,
optionally configures shared clock for inputs zero/one, and returns incidental
global address. Its sole caller is `zx_pon_probe`. Record:
`functions/0x00218-ponserdes_to_xmac1_en_set.md`.

### 0x00274 pon_sys_soft_reset
Status: complete. Pulses NPPT offset `0x2c0004` bit 31 low then high with fixed
delay and diagnostic logs. Its sole caller is `zx_pon_probe`. Record:
`functions/0x00274-pon_sys_soft_reset.md`.

### 0x00324 arm64_kernel_use_ng_mappings
Status: complete. Selects CPU capability index 23 from ready-time key state or
fallback HWCAP bit 23. Three `zx_pon_probe` callers use it. Record:
`functions/0x00324-arm64_kernel_use_ng_mappings.md`.

### 0x0035c pon_int_enable
Status: complete. Clears an input mask at PON offset `0x44`, stores and returns
the updated word. Eight interrupt registration helpers call it. Record:
`functions/0x0035c-pon_int_enable.md`.

### 0x00374 nppt_int_enable
Status: complete. Clears an input mask at NPPT offset `0x4`, stores and returns
the updated word. PTP, PTP timestamp, and OAM registration helpers call it.
Record: `functions/0x00374-nppt_int_enable.md`.

### 0x0038c pon_soc_pon_core_clk_init
Status: complete. CPU 133/129-gated PON core clock CRM configuration with
revision-specific bit fields. Its sole caller is `zx_pon_probe`. Record:
`functions/0x0038c-pon_soc_pon_core_clk_init.md`.

### 0x003e0 pon_soc_pon_cci_clk_init
Status: complete. Sets CRM offset `0x4` bits 4-5 and returns zero. No direct
code xrefs. Record: `functions/0x003e0-pon_soc_pon_cci_clk_init.md`.

### 0x003fc pon_soc_pon_woe0_clk_init
Status: complete. Sets CRM offset `0xc` bits covered by `0x00700000` and returns
zero. No direct code xrefs. Record:
`functions/0x003fc-pon_soc_pon_woe0_clk_init.md`.

### 0x00418 pon_soc_pon_woe1_clk_init
Status: complete. Sets CRM offset `0xc` bits covered by `0x07000000` and returns
zero. No direct code xrefs. Record:
`functions/0x00418-pon_soc_pon_woe1_clk_init.md`.

### 0x00434 pon_soc_pon_tm_clk_init
Status: complete. Sets CRM offset `0xc` bits zero and one, then returns zero. No
direct code xrefs. Record: `functions/0x00434-pon_soc_pon_tm_clk_init.md`.

### 0x00450 nppt_idm_cci_enable
Status: complete. Writes fixed IDM CCI enable value `0x00200020` at two
system-control offsets, then returns logging status. Its sole caller is
`zx_pon_probe`. Record: `functions/0x00450-nppt_idm_cci_enable.md`.

### 0x00480 pon_soc_pon_cci_aclk_init
Status: complete. Sets CRM offset `0x4` bits 4-6 and returns zero. Its sole
caller is `zx_pon_probe`. Record:
`functions/0x00480-pon_soc_pon_cci_aclk_init.md`.

### 0x0049c pon_soc_pon_tm_aclk_init
Status: complete. CPU 129 sets CRM offset `0xc` low two bits; other CPUs set low
three bits. Its sole caller is `zx_pon_probe`. Record:
`functions/0x0049c-pon_soc_pon_tm_aclk_init.md`.

### 0x004d4 pon_soc_pon_nppt_clk_init
Status: complete. Programs revision-specific NPPT CRM mux fields, enables bit
10 at CRM offset `0x48`, logs both resulting words, and returns zero. Its sole
caller is `zx_pon_probe`. Record:
`functions/0x004d4-pon_soc_pon_nppt_clk_init.md`.

### 0x00548 pon_soc_pon_woe_clk_init
Status: complete. Programs CPU-129-specific or general WOE clock bits at CRM
offset `0xc`. Its sole caller is `zx_pon_probe`. Record:
`functions/0x00548-pon_soc_pon_woe_clk_init.md`.

### 0x00dcc pon_soc_pon_rgmii_clk_set
Status: complete. Replaces CRM offset `0xc` bit 16 with the active-low predicate
`enable == 0`. Its sole caller is `nppt_smac_set_rgmii_mode`. Record:
`functions/0x00dcc-pon_soc_pon_rgmii_clk_set.md`.

### 0x00df4 pps_reset
Status: complete. Applies fixed reset/restore masks at PPS offset `0xc`, with a
fixed delay and diagnostic logs. No direct code xrefs. Record:
`functions/0x00df4-pps_reset.md`.

### 0x00eb4 pon_driver_unregister
Status: complete. No-argument void wrapper around
`platform_driver_unregister(&zx_pon_driver)`. Its sole caller is module cleanup.
Record: `functions/0x00eb4-pon_driver_unregister.md`.

### 0x00fa8 register_gmac_int
Status: complete. Stores a two-word ISR callback/context pair and returns
`pon_int_enable(1)`. No direct code xrefs; external registration API. Record:
`functions/0x00fa8-register_gmac_int.md`.

### 0x00fcc register_xgmac_int
Status: complete. Stores the XGPON ISR callback and shared context, then returns
`pon_int_enable(0x80)`. No direct code xrefs; external registration API. Record:
`functions/0x00fcc-register_xgmac_int.md`.

### 0x00ff4 register_emac_int
Status: complete. Stores the EPON ISR callback and shared context, then returns
`pon_int_enable(0x100)`. No direct code xrefs; external registration API. Record:
`functions/0x00ff4-register_emac_int.md`.

### 0x0101c register_xeumac_int
Status: complete. Stores the XEPON upstream ISR callback and shared context,
then returns `pon_int_enable(0x200)`. No direct code xrefs; external registration
API. Record: `functions/0x0101c-register_xeumac_int.md`.

### 0x01044 register_xedmac_int
Status: complete. Stores the XEPON downstream ISR callback and shared context,
then returns `pon_int_enable(0x400)`. No direct code xrefs; external registration
API. Record: `functions/0x01044-register_xedmac_int.md`.

### 0x0106c register_dg_int
Status: complete. Stores an opaque DGi handler and shared context, then returns
`pon_int_enable(0x20)`. The module does not dispatch that handler, so no callable
prototype is inferred. Record: `functions/0x0106c-register_dg_int.md`.

### 0x01094 register_lp_int
Status: complete. Stores the LP ISR callback and shared context, then returns
`pon_int_enable(0x40)`. No direct code xrefs; external registration API. Record:
`functions/0x01094-register_lp_int.md`.

### 0x010bc register_low_power_int
Status: complete. Stores the low-power ISR callback and shared context, then
returns `pon_int_enable(0x800)`. No direct code xrefs; external registration API.
Record: `functions/0x010bc-register_low_power_int.md`.

### 0x010e4 register_ptp_int
Status: complete. Stores the PTP ISR callback and shared context, then returns
`nppt_int_enable(0x400)`. No direct code xrefs; external registration API.
Record: `functions/0x010e4-register_ptp_int.md`.

### 0x0110c register_ptp_stamp_int
Status: complete. Stores the PTP timestamp ISR callback and shared context, then
returns `nppt_int_enable(0x200)`. No direct code xrefs; external registration
API. Record: `functions/0x0110c-register_ptp_stamp_int.md`.

### 0x01134 register_oam_int
Status: complete. Stores the OAM ISR callback without changing shared context,
then returns `nppt_int_enable(0x100)`. No direct code xrefs; external registration
API. Record: `functions/0x01134-register_oam_int.md`.

### 0x01154 dg_timer_func
Status: complete. Void timer callback that restores optical TX power, applies
four independent PON mode-specific register updates, and clears `dg_flag`.
Record: `functions/0x01154-dg_timer_func.md`.

### 0x011fc epon_set_dg_cnt
Status: complete. Void setter that independently transforms low counter nibbles
at two EPON/XEPON offsets according to `g_pon_work_mode`. Its sole caller is
`zx_pon_int`. Record: `functions/0x011fc-epon_set_dg_cnt.md`.

### 0x01258 zxic_gpio_set_value
Status: complete. One-instruction no-op stub with no direct code xrefs. Any
external ignored-argument convention remains unprovable. Record:
`functions/0x01258-zxic_gpio_set_value.md`.

### 0x0125c epon_get_llid_state
Status: complete. Returns PON offset `0x180004` bits 8-15. `zx_pon_int` and
`pon_is_registered` call it. Record:
`functions/0x0125c-epon_get_llid_state.md`.

### 0x01274 xepon_get_llid_state
Status: complete. Returns PON offset `0x1c0008` bits 8-15. `zx_pon_int` and
`pon_is_registered` call it. Record:
`functions/0x01274-xepon_get_llid_state.md`.

### 0x0128c xgpon_get_onu_state
Status: complete. Returns PON offset `0x59400` low three bits. `zx_pon_int` and
`pon_is_registered` call it. Record:
`functions/0x0128c-xgpon_get_onu_state.md`.

### 0x012a4 gpon_get_onu_state
Status: complete. Returns PON offset `0x94000` low three bits. `zx_pon_int` and
`pon_is_registered` call it. Record:
`functions/0x012a4-gpon_get_onu_state.md`.

### 0x015f4 pon_is_registered
Status: complete. Uses a cached fast path, otherwise evaluates independent
XGPON/GPON/EPON/XEPON registration predicates in ordered overwrite sequence.
Its sole caller is `cpu_net_tx`. Record:
`functions/0x015f4-pon_is_registered.md`.

### 0x01780 unregister_pon_int
Status: complete. Frees `g_pon_irq` using `&pon_int_info` as context. Its sole
caller is `zx_pon_remove`. Record:
`functions/0x01780-unregister_pon_int.md`.

### 0x017a8 unregister_nppt_int
Status: complete. Frees `g_nppt_irq` using `&pon_int_info` as context. Its sole
caller is `zx_pon_remove`. Record:
`functions/0x017a8-unregister_nppt_int.md`.

### 0x017d0 apb_write
Status: complete. Stores a 32-bit APB word and returns the unchanged address
pointer. No direct code xrefs. Record: `functions/0x017d0-apb_write.md`.

### 0x017d8 apb_read
Status: complete. Returns one volatile 32-bit APB word. No direct code xrefs.
Record: `functions/0x017d8-apb_read.md`.

### 0x017e0 apb_bit_write
Status: complete. Performs a dynamic-width APB field RMW without masking the
input value, then returns the unchanged address pointer. Eleven direct callers.
Record: `functions/0x017e0-apb_bit_write.md`.

### 0x01808 an1_pll_en_cfg
Status: complete. Clears PLL offset `0x10` bit zero, ORs an unmasked 32-bit
input, then stores and returns the updated word. No direct code xrefs. Record:
`functions/0x01808-an1_pll_en_cfg.md`.

### 0x01824 serdes_err_cnt_reset
Status: complete. Pulses SerDes offset `0x94` bit 15 low then high using two
separate RMWs. Two PRBS counter readers call it. Record:
`functions/0x01824-serdes_err_cnt_reset.md`.

### 0x0184c serdes_unlock
Status: complete. Clears SerDes offset `0x90` bits 13-14 and offset `0x40` bit
15. No direct code xrefs; recovered void action ABI. Record:
`functions/0x0184c-serdes_unlock.md`.

### 0x01870 an1_pll_en_get
Status: complete. Reads, logs, and returns PLL offset `0x10` bit zero. No direct
code xrefs. Record: `functions/0x01870-an1_pll_en_get.md`.

### 0x018ac an1_pll_bypass_cfg
Status: complete. Snapshots seven PLL/SerDes words, then performs mode-zero
writeback or mode-one/two bypass programming; invalid modes log without setup.
No direct code xrefs. Record: `functions/0x018ac-an1_pll_bypass_cfg.md`.

### 0x01a38 an1_pll_bypass_get
Status: complete. Reads, logs, and returns PLL offset `0xc` bit seven. It reports
bypass enabled state, not mode one versus two. No direct code xrefs. Record:
`functions/0x01a38-an1_pll_bypass_get.md`.

### 0x01a74 an1_pll_out_mode_cfg
Status: complete. Clears PLL offset `0x1c` bit one, ORs an unmasked input shifted
by one, then returns explanatory logging status. No direct code xrefs. Record:
`functions/0x01a74-an1_pll_out_mode_cfg.md`.

### 0x01aa8 an1_pll_out_mode_get
Status: complete. Reads PLL offset `0x1c` bit one, logs the corresponding
156.25/155.52 MHz mode, and returns the normalized bit. No direct code xrefs.
Record: `functions/0x01aa8-an1_pll_out_mode_get.md`.

### 0x01af0 an1_pll_cfg_ring_circle_bisa_set
Status: complete. Replaces PLL offset `0x4` bits 16-19 using an unmasked input
shifted by 16, then returns logging status. No direct code xrefs. Record:
`functions/0x01af0-an1_pll_cfg_ring_circle_bisa_set.md`.

### 0x01b28 an1_pll_cfg_ring_circle_bisa_get
Status: complete. Extracts, logs, and returns PLL offset `0x4` bits 16-19. No
direct code xrefs. Record:
`functions/0x01b28-an1_pll_cfg_ring_circle_bisa_get.md`.

### 0x01b64 an1_pll_cfg_ring_circle_resl_set
Status: complete. Replaces PLL offset `0x4` bits 23-26 using an unmasked input
shifted by 23, then returns logging status. No direct code xrefs. Record:
`functions/0x01b64-an1_pll_cfg_ring_circle_resl_set.md`.

### 0x01b9c an1_pll_cfg_ring_circle_resl_get
Status: complete. Extracts, logs, and returns PLL offset `0x4` bits 23-26. No
direct code xrefs. Record:
`functions/0x01b9c-an1_pll_cfg_ring_circle_resl_get.md`.

### 0x01bd8 com_pll_cfg_ring_circle_bisa_set
Status: complete. Replaces common SerDes offset `0x4` bits 16-19 using an
unmasked input, prints its value mapping, then returns data logging status. No
direct code xrefs. Record: `functions/0x01bd8-com_pll_cfg_ring_circle_bisa_set.md`.

### 0x01c28 com_pll_cfg_ring_circle_bisa_get
Status: complete. Extracts common SerDes offset `0x4` bits 16-19, logs the value
and mapping, then returns it. No direct code xrefs. Record:
`functions/0x01c28-com_pll_cfg_ring_circle_bisa_get.md`.

### 0x01c70 com_pll_cfg_ring_circle_resl_set
Status: complete. Replaces common SerDes offset `0x4` bits 23-26 using an
unmasked input shifted by 23, then returns logging status. No direct code xrefs.
Record: `functions/0x01c70-com_pll_cfg_ring_circle_resl_set.md`.

### 0x01ca8 com_pll_cfg_ring_circle_resl_get
Status: complete. Extracts, logs, and returns common SerDes offset `0x4` bits
23-26. No direct code xrefs. Record:
`functions/0x01ca8-com_pll_cfg_ring_circle_resl_get.md`.

### 0x01ce4 serdes_set_tx_swin
Status: complete. Replaces SerDes offset `0x20` bits 16-17 using an unmasked
input shifted by 16, then returns logging status. No direct code xrefs. Record:
`functions/0x01ce4-serdes_set_tx_swin.md`.

### 0x01d1c serdes_set_low_power
Status: complete. Dispatches modes 0-4 to low-byte values at SerDes offset
`0x5c`; mode 5 and values above 5 have distinct no-write error logs. Every
path returns logging status. No direct xrefs. Record:
`functions/0x01d1c-serdes_set_low_power.md`.

### 0x01e28 serdes_set_band
Status: complete. Sequentially replaces SerDes offset `0x6c` bit 14 and its
low byte using unmasked inputs, then returns logging status. No direct xrefs.
Record: `functions/0x01e28-serdes_set_band.md`.

### 0x01e6c serdes_get_band
Status: complete. Extracts SerDes offset `0xd0` bits 16-23, logs the byte, and
returns it. This does not read the setter's `0x6c` register. No direct xrefs.
Record: `functions/0x01e6c-serdes_get_band.md`.

### 0x01ea8 serdes_set_gen_en
Status: complete. Replaces SerDes offset `0x94` bit 13 using an unmasked input
and returns logging status. The TX PRBS mode handler calls it with 1 and 0.
Record: `functions/0x01ea8-serdes_set_gen_en.md`.

### 0x01ee0 serdes_set_check_en
Status: complete. Replaces SerDes offset `0x94` bit 14 using an unmasked input
and returns logging status. RX-BIST paths forward an enable or pass 1. Record:
`functions/0x01ee0-serdes_set_check_en.md`.

### 0x01f18 serdes_set_err_cnt_en
Status: complete. Replaces SerDes offset `0x94` bit 15 using an unmasked input
and returns logging status. RX-BIST paths forward an enable or pass 1. Record:
`functions/0x01f18-serdes_set_err_cnt_en.md`.

### 0x01f50 serdes_get_err_cnt
Status: complete. Combines offset `0xe8` bits 0-31 with offset `0xec` bits
0-15 into an unsigned 48-bit PRBS error count, logs it, and returns it. Record:
`functions/0x01f50-serdes_get_err_cnt.md`.

### 0x01f98 serdes_prbs_err_ok
Status: complete. Sets SerDes offset `0x48` bit 23 and returns the one-bit
error log status; despite its name, it is a command rather than a predicate.
No direct xrefs. Record: `functions/0x01f98-serdes_prbs_err_ok.md`.

### 0x01fc8 serdes_set_error_time
Status: complete. Converts seconds with a mode-dependent 156250000 or
155520000 multiplier, writes the low 32 bits at `0x98`, clears the apparent
high byte at `0xa4`, logs the truncated readback, and returns logging status.
Record: `functions/0x01fc8-serdes_set_error_time.md`.

### 0x02048 serdesPrbsCounterGetHandler
Status: complete. Void timer callback that logs overflow when the current PRBS
count is below `serdesPrbsCounter`, otherwise logs their difference. It does
not update the baseline or rearm the timer. Record:
`functions/0x02048-serdesPrbsCounterGetHandler.md`.

### 0x0208c serdes_set_loopback_mode
Status: complete. Snapshots/restores six SerDes registers, applies distinct
ordered transactions for loopback modes 0-8, treats mode 9 as recovery and
mode 10 as a counted error, and rejects values above 10 without incrementing
its static counter. No direct xrefs. Record:
`functions/0x0208c-serdes_set_loopback_mode.md`.

### 0x02874 serdes_set_rx_eq_mbf
Status: complete. Replaces SerDes offset `0x2c` bits 18-21 using an unmasked
input and returns logging status. No direct xrefs. Record:
`functions/0x02874-serdes_set_rx_eq_mbf.md`.

### 0x028ac serdes_get_rx_eq
Status: complete. Reads offset `0x2c` once, reports three active-low EQ enables
and their five-bit fields plus four-bit MBF, then returns the final log status.
No direct xrefs. Record: `functions/0x028ac-serdes_get_rx_eq.md`.

### 0x02970 serdes_set_np_jittery
Status: complete. Replaces SerDes offset `0x48` bits 6-8 using an unmasked
input and returns logging status. No direct xrefs. Record:
`functions/0x02970-serdes_set_np_jittery.md`.

### 0x029a8 serdes_get_np_jittery
Status: complete. Reads offset `0x48` once, extracts bits 6-8, logs, and
returns the three-bit value. No direct xrefs. Record:
`functions/0x029a8-serdes_get_np_jittery.md`.

### 0x029e4 check_serdes_version
Status: complete. Reads version selector `0x4[4:1]` and legacy signature
`0x18[31:16]`, logs V1/V2/V3 or error with binary-defined precedence, and
returns logging status. No direct xrefs. Record:
`functions/0x029e4-check_serdes_version.md`.

### 0x02a58 check_serdes_config
Status: complete. Prints the current mode label, dumps 74 SerDes words, and on
CPU 132 dumps 10 additional PLL words. Incoherent residual `w0` values are not
a semantic return; recovered ABI is void. No direct xrefs. Record:
`functions/0x02a58-check_serdes_config.md`.

### 0x02bf0 serdes_set_tx_prbs_mode
Status: complete. Enables the PRBS generator, applies CPU-132/133/129 setup,
then selects PRBS7/23/31/9/15 or a fixed `0101` pattern for modes 0-5. All
inputs return zero; no direct xrefs. Record:
`functions/0x02bf0-serdes_set_tx_prbs_mode.md`.

### 0x02de8 serdes_set_rx_prbs_mode
Status: complete. Applies CPU-132/133/129 setup, then maps modes 0-4 to
PRBS7/23/31/9/15 in offset `0x94` bits 19-21. Invalid modes retain common
setup; all inputs return zero. Record:
`functions/0x02de8-serdes_set_rx_prbs_mode.md`.

### 0x02f90 serdes_set_sprbsrxbist
Status: complete. Passes 32-bit `prbs_mode - 1` to RX PRBS selection, forwards
one enable to the checker and error counter, then returns the final log result.
No direct xrefs. Record: `functions/0x02f90-serdes_set_sprbsrxbist.md`.

### 0x02fe4 serdes_set_pattern
Status: complete. Loads an 80-bit pattern across offsets `0x9c..0xa4`, applies
CPU-133/129 setup, and sets or clears `0xa4[18:16]`. Residual base pointer is
not a semantic return; ABI is void. No direct xrefs. Record:
`functions/0x02fe4-serdes_set_pattern.md`.

### 0x03098 check_serdes_lock
Status: complete. Independently reports CPU-132/133/129 PLL, CDR, and ALOS raw
status bits, with CPU-specific PLL offsets. Incoherent residual `w0` is not a
semantic return; ABI is void. No direct xrefs. Record:
`functions/0x03098-check_serdes_lock.md`.

### 0x03160 get_all_efuse
Status: complete
Confidence: verified CPU gate, all raw dumps, per-field bit extraction, read
order, and constant zero return.
Role: Full efuse diagnostic dump. One `isCpuType_129()` test selects between two
different decode layouts, each printing 33 raw words at `efuse_base+0x00..0x80`
followed by ATE, board-protection, reserved-protect, ATE-IP, and status
sections.
Inputs/outputs: No parameters; always returns 0 from a shared tail.
Globals/MMIO/callbacks: Reads `efuse_base @ 0x27680` only; re-reads each word
per printed value, 98 reads on the 129 branch and 99 on the other, with the
reserved-protect word read 16 times. No writes.
Concurrency/ownership: Purely diagnostic, no locking, no state changes.
Security: Prints raw AES secret key plus HUK and HASH key (129) or backup AES
key (other CPUs) to the kernel log.
Evidence: 850-instruction body at `0x3160`-`0x410a`, branch gate at `0x3190`,
`EXTR`/`UBFIZ`/`UBFX` field decodings, 9-value HASH-key call with a stack
argument, shared `MOV W0, #0` exit at `0x40ec`, and a per-branch call/read
census matching the reconstruction.
Open questions: Meaning of `Chip_Type`, `PON_mode`, and `Bin` encodings; whether
any external debug hook still calls this entry.
Recovered source: `recovered/plat_smac.c`; detailed record:
`functions/0x03160-get_all_efuse.md`.

### 0x0410c serdes_set_tx_eq
Status: complete. Exported two-level TX equalization setter. Value 0 replaces
SerDes `0x20[15:8]` with `0x0d`; value 1 replaces it with `0x1d`; every other
value is a no-op. Both accepted paths log and all paths return 0. No direct
IDB xrefs; vendor kallsyms includes its `__ksymtab` entry. Record:
`functions/0x0410c-serdes_set_tx_eq.md`.

### 0x0417c serdes_set_pll_open_loop
Status: complete. Exported PLL open-loop setter. Exact input 1 sets SerDes
`0x68[10:9]`, `0x68[22]`, and `0x74[13]`; every other value clears them. It
performs two distinct RMWs of `0x68`, then RMWs `0x74`, and returns the final
log result. No direct IDB xrefs; vendor kallsyms includes its `__ksymtab`
entry. Record: `functions/0x0417c-serdes_set_pll_open_loop.md`.

### 0x04200 serdes_set_clk_change
Status: complete. Exported RX-to-TX clock selector. Zero clears SerDes
`0x48[18]` for the vendor-named `looptiming` clock; every nonzero input sets it
for local clock. Each path performs one RMW and returns its `printk` result.
No direct IDB xrefs; vendor kallsyms includes its `__ksymtab` entry. Record:
`functions/0x04200-serdes_set_clk_change.md`.

### 0x0424c serdes_set_rx_eq1
Status: complete. Exported EQ1 setter. Values greater than 1 only log an error;
zero disables EQ1 through `0x2c[0]`; one enables it and replaces `0x2c[7:3]`
with an unmasked shifted input. Its residual machine returns are incoherent, so
the recovered semantic ABI is void. No direct IDB xrefs; vendor kallsyms
includes its `__ksymtab` entry. Record: `functions/0x0424c-serdes_set_rx_eq1.md`.

### 0x042b0 serdes_set_rx_eq2
Status: complete. Exported EQ2 setter. Values greater than 1 only log an error;
zero disables through `0x2c[1]`; one enables it and replaces `0x2c[12:8]` with
an unmasked shifted input. Its machine return values are incoherent, so the
recovered semantic ABI is void. No direct IDB xrefs; vendor kallsyms includes
its `__ksymtab` entry. Record: `functions/0x042b0-serdes_set_rx_eq2.md`.

### 0x04314 serdes_set_rx_eq3
Status: complete. Exported EQ3 setter. Values greater than 1 only log an error;
zero disables through `0x2c[2]`; one enables it and replaces `0x2c[17:13]`
with an unmasked shifted input. Its machine return values are incoherent, so
the recovered semantic ABI is void. No direct IDB xrefs; vendor kallsyms
includes its `__ksymtab` entry. Record: `functions/0x04314-serdes_set_rx_eq3.md`.

### 0x04378 serdes_set_lane_mode
Status: complete. Exported CPU-133-only lane-mode setter. It clears SerDes
`0x94[2:0]` then ORs an unmasked input; other CPU types have no MMIO side
effects. Residual predicate/register values make the semantic ABI void. No
direct IDB xrefs; vendor kallsyms includes its `__ksymtab` entry. Record:
`functions/0x04378-serdes_set_lane_mode.md`.

### 0x043b8 serdes_set_error_time_en
Status: complete. Module-private error-time control. Clears `0x94[30]`, ORs an
unmasked input shifted left 30, logs, and returns the log result. Called with 1
by hard PRBS sampling and 0 by timer-based PRBS counting. Record:
`functions/0x043b8-serdes_set_error_time_en.md`.

### 0x043ec serdes_get_hard_prbs_cnt
Status: complete. Exported synchronous hard PRBS measurement. Resets/configures
the checker, busy-waits `((uint32_t)(seconds * 1000))` times with
`__const_udelay(0x418958)`, adds the sampled 64-bit error count to
`iPrbsCounter`, and returns the final log result. No direct IDB xrefs; vendor
kallsyms includes its `__ksymtab` entry. Record:
`functions/0x043ec-serdes_get_hard_prbs_cnt.md`.

### 0x04490 serdes_get_prbs_counters
Status: complete. Exported timer-based PRBS counter setup. Always disables
error-time control; an RXBIST/LOS state only logs and returns zero. Otherwise
it resets the timer with the PRBS callback, sets `expires` to
`jiffies + (uint32_t)(time * 100)`, establishes an error-count baseline, and
returns zero. No direct IDB xrefs; vendor kallsyms includes its `__ksymtab`
entry. Record: `functions/0x04490-serdes_get_prbs_counters.md`.

### 0x04560 mode_epon_cfg
Status: complete. Mode-0 EPON SerDes configuration script. CPU 132 has priority
and receives one 46-word raw profile; CPU 133 receives a different 46-word
profile; either receives three common tail stores. Other CPUs only log. Return
register residuals are incoherent, so the recovered semantic ABI is void. Sole
caller: `serdes_mode_set` switch case 0. Record:
`functions/0x04560-mode_epon_cfg.md`.

### 0x04908 mode_10g_epon_nsyn_dpll_cfg
Status: complete. Mode-1 non-synchronous 10G EPON DPLL SerDes script. CPU 132
and CPU 133 receive distinct 46-word raw profiles plus three common tail
stores; other CPUs only log. The dispatcher's two passed arguments are ignored,
and residual returns make the semantic ABI void. Sole caller: `serdes_mode_set`
switch case 1. Record: `functions/0x04908-mode_10g_epon_nsyn_dpll_cfg.md`.

### 0x04cc4 mode_10g_epon_nsyn_fifo_cfg
Status: complete. Mode-2 non-synchronous 10G EPON FIFO SerDes script. CPU 132
and CPU 133 receive distinct 46-word raw profiles plus three common tail
stores; other CPUs only log. Dispatcher arguments are ignored and residual
returns make the semantic ABI void. Sole caller: `serdes_mode_set` switch case
2. Record: `functions/0x04cc4-mode_10g_epon_nsyn_fifo_cfg.md`.

### 0x05080 mode_10g_epon_nsyn_nofifo_cfg
Status: complete. Mode-3 non-synchronous 10G EPON no-FIFO SerDes script. CPU
132 and CPU 133 receive distinct 46-word raw profiles plus three common tail
stores; other CPUs only log. Dispatcher arguments are ignored and residual
returns make the semantic ABI void. Sole caller: `serdes_mode_set` switch case
3. Record: `functions/0x05080-mode_10g_epon_nsyn_nofifo_cfg.md`.

### 0x0543c mode_10g_epon_syn_cfg
Status: complete. Mode-4 synchronous 10G EPON SerDes script. CPU-133-only;
matching hardware receives 49 ordered raw 32-bit stores, other CPUs only log.
Dispatcher arguments are ignored and residual returns make the semantic ABI
void. Sole caller: `serdes_mode_set` switch case 4. Record:
`functions/0x0543c-mode_10g_epon_syn_cfg.md`.

### 0x05644 mode_gpon_cfg
Status: complete. Mode-5 GPON SerDes script. It selects CPU 129, then 132, then
133 profiles; each writes 46 profile words plus three common tail words. Other
CPUs only log. Dispatcher arguments are ignored and residual returns make the
semantic ABI void. Sole caller: `serdes_mode_set` switch case 5. Record:
`functions/0x05644-mode_gpon_cfg.md`.

### 0x05ba8 mode_gpon_syn_cfg
Status: complete. CPU-129-only synchronous GPON profile. It writes 49 raw
32-bit SerDes words when supported and otherwise only logs the inherited
`mode_gpon_cfg` string. No direct IDB xrefs; residual returns make the semantic
ABI void. Record: `functions/0x05ba8-mode_gpon_syn_cfg.md`.

### 0x05d90 mode_xgpon_nsyn_cfg
Status: complete. Mode-6 non-synchronous XGPON SerDes script. CPU 132 takes a
44-word profile with two retained nested CPU-133 conditional writes; CPU 133
takes a 46-word fallback profile. Both supported paths append three common
32-bit tail writes. Its sole direct caller is `serdes_mode_set` case 6, and its
semantic ABI is void. Record: `functions/0x05d90-mode_xgpon_nsyn_cfg.md`.

### 0x06174 mode_xgpon_syn_cfg
Status: complete. Mode-7 synchronous XGPON SerDes script. It is CPU-133-only
and performs 49 ordered 32-bit stores, three of which select raw alternatives
from a one-time `product_vid` byte predicate. Its sole direct caller is
`serdes_mode_set` case 7; residual returns make its semantic ABI void. Record:
`functions/0x06174-mode_xgpon_syn_cfg.md`.

### 0x063ec eth_an1_clk_set
Status: complete. Programs a fixed eight-word Ethernet AN1 PLL profile, RMWs
enable bit 0 at `+0x10`, then polls lock bit 0 at `+0x20` with 1001 delayed
retries. It always logs completion and returns that final log status. Sole
caller: `an1_pll_clk_set` modes 8-16. Record:
`functions/0x063ec-eth_an1_clk_set.md`.

### 0x064c0 an1_pll_epon_cfg
Status: complete. Programs the same eight-word AN1 PLL profile and enable RMW
as the Ethernet entry, but polls using `0x418958` delays and EPON log strings.
It returns the final completion-log status. Sole caller: `an1_pll_clk_set`
modes 0-4. Record: `functions/0x064c0-an1_pll_epon_cfg.md`.

### 0x06594 an1_pll_gpon_cfg
Status: complete. Programs the GPON AN1 PLL variant (`+0x08=0x01050700`,
`+0x1c=0x00130000`), RMWs enable bit zero, and polls lock with 1001 long-delay
retries. It returns the final completion-log status. Sole caller:
`an1_pll_clk_set` modes 5-7. Record: `functions/0x06594-an1_pll_gpon_cfg.md`.

### 0x06664 mode_eth_10gbase_r_cfg
Status: complete. Two-profile Ethernet SerDes script: CPU 132 and CPU 133 each
receive 46 raw ordered words followed by the same three 32-bit tail writes.
`serdes_mode_set` shares it between cases 13 (`MODE_ETH_10GBASE_R`) and 14
(`MODE_ETH_USXGMII_10G`); residual returns make its semantic ABI void. Record:
`functions/0x06664-mode_eth_10gbase_r_cfg.md`.

### 0x06a28 mode_eth_5gbase_r_cfg
Status: complete. Two-profile Ethernet 5GBASE-R script: CPU 132 and CPU 133
each receive 46 raw ordered words followed by the common three-word tail.
`serdes_mode_set` shares it between cases 11 and 12; diagnostic labels only
name case 12 as `MODE_ETH_USXGMII_5G`. Semantic ABI is void. Record:
`functions/0x06a28-mode_eth_5gbase_r_cfg.md`.

### 0x06df0 mode_eth_2p5gbase_r_cfg
Status: complete. Ethernet 2.5GBASE-R script with a CPU-132 profile and a
shared CPU-133/129 profile; each has 46 ordered words plus the common three-word
tail. CPU-133 is tested before CPU-129. Sole caller: `serdes_mode_set` case 10
(`MODE_ETH_USXGMII_2P5G`); semantic ABI is void. Record:
`functions/0x06df0-mode_eth_2p5gbase_r_cfg.md`.

### 0x071c8 mode_eth_2p5gbase_x_cfg
Status: complete. Ethernet 2.5GBASE-X script with distinct CPU-132, CPU-133,
and CPU-129 49-word profiles in that priority order. `serdes_mode_set` shares
it between cases 9 (`MODE_ETH_HSGMII`) and 15 (`MODE_ETH_2P5BASE_X`); residual
returns make its semantic ABI void. Record:
`functions/0x071c8-mode_eth_2p5gbase_x_cfg.md`.

### 0x07728 mode_eth_1gbase_x_cfg
Status: complete. Ethernet 1GBASE-X script with distinct CPU-132, CPU-133, and
CPU-129 49-word profiles in that priority order. `serdes_mode_set` shares it
between cases 8 (`MODE_ETH_SGMII`) and 16 (`MODE_ETH_1GBASE_X`); residual
returns make its semantic ABI void. Record:
`functions/0x07728-mode_eth_1gbase_x_cfg.md`.

### 0x07c74 an1_pll_clk_set
Status: complete. Bounded mode-bit dispatcher: modes 0-4 select the EPON AN1
PLL profile, 5-7 the GPON profile, and 8-16 the Ethernet profile; the selected
child `int` is propagated. Sole caller: CPU-132 `pon_serdes_init`. Record:
`functions/0x07c74-an1_pll_clk_set.md`.

### 0x07cc0 serdes_mode_set
Status: complete. Mode 0-16 jump-table dispatcher for all PON and Ethernet
SerDes profiles, with shared 8/16, 9/15, 11/12, and 13/14 cases. Values above
16 are no-ops. Its sole caller discards incidental return residuals, so the
semantic ABI is void. Record: `functions/0x07cc0-serdes_mode_set.md`.

### 0x07d58 pon_serdes_init
Status: complete. Runs CPU-132 AN1 setup, mode profile dispatch, three SerDes
RMWs, common PLL lock polling, LOS/CDR checks, and an additional CPU-129 reset
state poll. It returns `-1` on any timeout or zero on success. Sole caller:
`zx_pon_clk_reset_init`. Record: `functions/0x07d58-pon_serdes_init.md`.

### 0x07f20 pon_pll_cfg
Status: complete. Mode-range CRM PLL programming for EPON (0-4), GPON (5-7),
and Ethernet (8-16), with CPU-132-specific profile ordering and common final
`+0xc` RMWs. Invalid modes are no-ops; returns zero. Sole caller:
`zx_pon_clk_reset_init`. Record: `functions/0x07f20-pon_pll_cfg.md`.

### 0x08088 zx_pon_clk_reset_init
Status: complete. Exported PON clock/reset entry: configures the mode PLL,
records `pon_serdes_mode`, pulses CRM reset bits by CPU type, then invokes
`pon_serdes_init` and logs its outcome. Its semantic ABI is void. Record:
`functions/0x08088-zx_pon_clk_reset_init.md`.

### 0x0829c uni_apb_write
Status: complete. Exported APB helper that performs one volatile 32-bit write
and returns the input pointer unchanged. No internal IDB xrefs. Record:
`functions/0x0829c-uni_apb_write.md`.

### 0x082a4 uni_apb_read
Status: complete. Exported APB helper that returns one volatile zero-extended
32-bit read. No internal IDB xrefs. Record:
`functions/0x082a4-uni_apb_read.md`.

### 0x082ac uni_apb_bit_write
Status: complete. Exported APB field RMW helper. It clears a width/shift-derived
field, ORs an unmasked shifted value, writes one volatile 32-bit word, and
returns the input pointer. No internal IDB xrefs. Record:
`functions/0x082ac-uni_apb_bit_write.md`.

### 0x082d4 uni_serdes_err_cnt_reset
Status: complete. Exported Uni SerDes counter-reset pulse: clear then set bit
15 at `+0x94` through two RMWs and return zero. Called by both Uni SerDes PRBS
counter readers. Record: `functions/0x082d4-uni_serdes_err_cnt_reset.md`.

### 0x082fc uni_serdes_set_pattern
Status: complete. Exported Uni SerDes pattern transaction: clears `+0x94` bits
12-15, programs `+0x9c/+0xa0`, replaces `+0xa4` low 16 bits, then exactly
enables or clears its bits 16-18 and returns the final word. No internal IDB
xrefs. Record: `functions/0x082fc-uni_serdes_set_pattern.md`.

### 0x0834c zx_uni_clk_reset_init
Status: complete. Exported Uni clock/reset stub that always returns zero and
has no internal IDB xrefs. Record: `functions/0x0834c-zx_uni_clk_reset_init.md`.

### 0x08354 uni_com_pll_cfg_ring_circle_bisa_set
Status: complete. Exported Uni common-PLL setter: clears `+0x04` bits 16-19,
ORs an unmasked input shifted by 16, emits two vendor logs, and returns the
second log status. No internal IDB xrefs. Record:
`functions/0x08354-uni_com_pll_cfg_ring_circle_bisa_set.md`.

### 0x083a4 uni_com_pll_cfg_ring_circle_bisa_get
Status: complete. Exported getter that extracts `uni_serdes_base + 0x04` bits
16-19, logs the field and its mapping, then returns the four-bit value. Record:
`functions/0x083a4-uni_com_pll_cfg_ring_circle_bisa_get.md`.

### 0x083ec uni_com_pll_cfg_ring_circle_resl_set
Status: complete. Exported Uni common-PLL setter: clears `+0x04` bits 23-26,
ORs an unmasked input shifted by 23, logs the raw value, and returns the log
status. Record: `functions/0x083ec-uni_com_pll_cfg_ring_circle_resl_set.md`.

### 0x08424 uni_com_pll_cfg_ring_circle_resl_get
Status: complete. Exported getter that extracts `uni_serdes_base + 0x04` bits
23-26, logs the four-bit value, and returns it. No internal IDB xrefs. Record:
`functions/0x08424-uni_com_pll_cfg_ring_circle_resl_get.md`.

### 0x08460 uni_serdes_set_tx_swin
Status: complete. Exported Uni SerDes setter that clears `+0x20` bits 16-17,
ORs an unmasked input shifted by 16, logs the raw value, and returns the log
status. No internal IDB xrefs. Record:
`functions/0x08460-uni_serdes_set_tx_swin.md`.

### 0x08498 uni_serdes_set_low_power
Status: complete. Exported Uni SerDes low-power selector: modes 0-4 manipulate
the `+0x5c` low byte and return their logs; mode 5 and values above 5 are
separate log-only error paths. No internal IDB xrefs. Record:
`functions/0x08498-uni_serdes_set_low_power.md`.

### 0x085a4 uni_serdes_set_band
Status: complete. Exported Uni SerDes PLL-band setter with two ordered `+0x6c`
RMWs: raw input shifted by 14, then raw low-byte replacement. It returns the
completion log status and has no internal IDB xrefs. Record:
`functions/0x085a4-uni_serdes_set_band.md`.

### 0x085e8 uni_serdes_get_band
Status: complete. Exported Uni SerDes reader that extracts `+0xd0` bits 16-23,
logs and returns the byte. It intentionally reads a different register from
`uni_serdes_set_band`; no internal IDB xrefs. Record:
`functions/0x085e8-uni_serdes_get_band.md`.

### 0x08624 uni_serdes_set_gen_en
Status: complete. Exported Uni SerDes PRBS generator setter: clears `+0x94` bit
13, ORs an unmasked input shifted by 13, and returns its log status. Called by
`uni_serdes_set_tx_prbs_mode`. Record:
`functions/0x08624-uni_serdes_set_gen_en.md`.

### 0x0865c uni_serdes_set_check_en
Status: complete. Exported Uni SerDes PRBS checker setter: clears `+0x94` bit
14, ORs an unmasked input shifted by 14, and returns its log status. Called by
RX BIST and hard PRBS counter paths. Record:
`functions/0x0865c-uni_serdes_set_check_en.md`.

### 0x08694 uni_serdes_set_err_cnt_en
Status: complete. Exported Uni SerDes PRBS error-counter setter: clears `+0x94`
bit 15, ORs an unmasked input shifted by 15, and returns its log status. Called
by RX BIST and hard PRBS counter paths. Record:
`functions/0x08694-uni_serdes_set_err_cnt_en.md`.

### 0x086cc uni_serdes_get_err_cnt
Status: complete. Exported Uni SerDes PRBS counter getter: combines `+0xe8` and
the low 16 bits of `+0xec` into a 48-bit value, logs, and returns it. Used by
timer, hard-counter, and counter-snapshot paths. Record:
`functions/0x086cc-uni_serdes_get_err_cnt.md`.

### 0x08714 uni_serdes_prbs_err_ok
Status: complete. Exported Uni SerDes PRBS helper that RMW-sets `+0x48` bit 23,
logs success, and returns the log status. No internal IDB xrefs. Record:
`functions/0x08714-uni_serdes_prbs_err_ok.md`.

### 0x08744 uni_serdes_set_error_time_en
Status: complete. Exported Uni-named error-time setter that actually RMWs
`pon_serdes_base + 0x94` bit 30 with unmasked input shifted by 30. Called by
both Uni PRBS counter readers. Record:
`functions/0x08744-uni_serdes_set_error_time_en.md`.

### 0x08778 uni_serdes_set_error_time
Status: complete. Exported Uni SerDes error-time setter: maps mode 0-4 to a
156.25 MHz multiplier and modes 5-7/17 to 155.52 MHz, writes `+0x98`, clears
`+0xa4`'s high byte, and returns the final log status. Called by hard PRBS
counter setup. Record: `functions/0x08778-uni_serdes_set_error_time.md`.

### 0x087fc uni_serdesPrbsCounterGetHandler
Status: complete. Reads the Uni SerDes error counter and logs the unsigned
difference from `uni_serdesPrbsCounter`, or an overflow message when current
count is lower. Referenced as data by `uni_serdes_get_prbs_counters`. Record:
`functions/0x087fc-uni_serdesPrbsCounterGetHandler.md`.

### 0x08840 uni_serdes_set_rx_eq_mbf
Status: complete. Exported Uni SerDes RX EQ MBF setter: clears `+0x2c` bits
18-21, ORs an unmasked input shifted by 18, and returns its log status. No
internal IDB xrefs. Record: `functions/0x08840-uni_serdes_set_rx_eq_mbf.md`.

### 0x08878 uni_serdes_get_rx_eq
Status: complete. Exported Uni SerDes RX EQ diagnostic: snapshots `+0x2c`,
reports EQ1/2/3 enable state and values, always reports MBF, and returns the
final log status. No internal IDB xrefs. Record:
`functions/0x08878-uni_serdes_get_rx_eq.md`.

### 0x0893c uni_serdes_set_np_jittery
Status: complete. Exported Uni SerDes NP jitter setter: clears `+0x48` bits 6-8,
ORs an unmasked input shifted by 6, and returns its log status. No internal IDB
xrefs. Record: `functions/0x0893c-uni_serdes_set_np_jittery.md`.

### 0x08974 uni_serdes_get_np_jittery
Status: complete. Exported Uni SerDes NP jitter getter: extracts `+0x48` bits
6-8, logs and returns the three-bit value. No internal IDB xrefs. Record:
`functions/0x08974-uni_serdes_get_np_jittery.md`.

### 0x089b0 pin_mux_debug_clk_133_out0
Status: complete. Exported debug-clock output transaction: ordered raw pin-mux
and CRM RMWs program low, bits 16-18, and bits 20-21 fields, then log their
observed values. No internal IDB xrefs. Record:
`functions/0x089b0-pin_mux_debug_clk_133_out0.md`.

### 0x08a68 pin_mux_debug_clk_133_out1
Status: complete. Exported debug-clock output transaction: ordered raw pin-mux
and CRM RMWs program pin bits 12-14 plus CRM bits 24-26 and 8-11, then log
observed values. No internal IDB xrefs. Record:
`functions/0x08a68-pin_mux_debug_clk_133_out1.md`.

### 0x08b08 uni_check_serdes_config
Status: complete. Exported Uni SerDes mode diagnostic and 74-word register dump.
Mode 2 intentionally shares the error path with invalid values; it logs the
fixed `0x16100000..0x16100124` address range and returns the final separator
log status. Record: `functions/0x08b08-uni_check_serdes_config.md`.

### 0x08c00 uni_serdes_set_loopback_mode
Status: complete. Exported CPU-133 loopback state machine. It snapshots seven
default registers once, restores them before later calls, applies profiles 0-8,
handles default/error modes 9/10, and returns a persistent counter for valid
modes. Record: `functions/0x08c00-uni_serdes_set_loopback_mode.md`.

### 0x0941c uni_serdes_set_tx_prbs_mode
Status: complete. Exported TX PRBS selector. It always enables the generator,
applies CPU-133/129 setup, selects PRBS7/23/31 for modes 0/1/2, and returns
zero. No internal IDB xrefs. Record:
`functions/0x0941c-uni_serdes_set_tx_prbs_mode.md`.

### 0x09560 uni_serdes_set_rx_prbs_mode
Status: complete. Exported RX PRBS selector. It applies CPU-133/129 RX setup,
selects PRBS7/23/31 for modes 0/1/2 at a distinct field, and returns zero.
Called by RX BIST and hard PRBS counter paths. Record:
`functions/0x09560-uni_serdes_set_rx_prbs_mode.md`.

### 0x096a8 uni_serdes_set_sprbsrxbist
Status: complete. Exported RX BIST state machine. It snapshots default words
once, enables RX PRBS/check/counter only for exact enable value one, restores
otherwise, and returns a persistent call counter. The `+0x24` snapshot restores
to `+0x14` by binary behavior. Record:
`functions/0x096a8-uni_serdes_set_sprbsrxbist.md`.

### 0x09784 uni_check_serdes_lock
Status: complete. Exported SerDes lock diagnostic: selects a CPU-dependent PLL
status source, separately reads CDR and ALOS status, logs all three, and returns
the log status. No internal IDB xrefs. Record:
`functions/0x09784-uni_check_serdes_lock.md`.

### 0x097dc uni_serdes_get_hard_prbs_cnt
Status: complete. Exported hard PRBS counter transaction: configures RX PRBS,
runs `time_units * 1000` timed delays, accumulates the 48-bit count into
`uni_iPrbsCounter`, and returns its log status. Record:
`functions/0x097dc-uni_serdes_get_hard_prbs_cnt.md`.

### 0x09880 uni_serdes_get_prbs_counters
Status: complete. Exported Uni PRBS counter snapshot path: preserves a volatile
status read, schedules its reporting callback after a 32-bit `time * 100` tick
interval, resets and snapshots the counter baseline, then returns zero. Record:
`functions/0x09880-uni_serdes_get_prbs_counters.md`.

### 0x09940 uni_serdes_reset
Status: complete. Exported two-lane Uni SerDes reset command. It logs invalid
unsigned selectors, otherwise performs exact ordered APB bit sequences for
enable or disable, including both vendor delay constants. Record:
`functions/0x09940-uni_serdes_reset.md`.

### 0x09ae8 uni_serdes_set_tx_eq
Status: complete. Exported two-level Uni TX equalization setter. Value zero or
one replaces `uni_serdes_base + 0x20[15:8]` with `0x0d` or `0x1d`;
other values are no-ops and all paths return zero. Record:
`functions/0x09ae8-uni_serdes_set_tx_eq.md`.

### 0x09b58 uni_serdes_set_pll_open_loop
Status: complete. Exported Uni PLL open-loop setter. Exact input one sets
`+0x68[10:9]`, `+0x68[22]`, and `+0x74[13]`; every other value clears them,
preserving the two separate `+0x68` RMWs and final log return. Record:
`functions/0x09b58-uni_serdes_set_pll_open_loop.md`.

### 0x09bdc uni_serdes_set_clk_change
Status: complete. Exported Uni RX-to-TX clock selector. Zero clears
`uni_serdes_base + 0x48[18]`; every nonzero value sets it. Each path returns
its final log result. Record:
`functions/0x09bdc-uni_serdes_set_clk_change.md`.

### 0x09c28 uni_serdes_set_rx_eq1
Status: complete. Exported Uni RX-EQ1 setter. Exact zero disables by setting
`+0x2c[0]`; exact one enables and replaces bits 7:3 with an unmasked shifted
value; larger enables log and do nothing. Record:
`functions/0x09c28-uni_serdes_set_rx_eq1.md`.

### 0x09c8c uni_serdes_set_rx_eq2
Status: complete. Exported Uni RX-EQ2 setter. Exact zero disables by setting
`+0x2c[1]`; exact one enables and replaces bits 12:8 with an unmasked shifted
value; larger enables log and do nothing. Record:
`functions/0x09c8c-uni_serdes_set_rx_eq2.md`.

### 0x09cf0 uni_serdes_set_rx_eq3
Status: complete. Exported Uni RX-EQ3 setter. Exact zero disables by setting
`+0x2c[2]`; exact one enables and replaces bits 17:13 with an unmasked shifted
value; larger enables log and do nothing. Record:
`functions/0x09cf0-uni_serdes_set_rx_eq3.md`.

### 0x09d54 uni_mode_eth_10gbase_r_cfg
Status: complete. Internal Uni 10GBASE-R profile writer. It logs then performs
49 unconditional ordered 32-bit stores at `uni_serdes_base + 0x00..0xc0` with
no CPU predicate, read, or RMW. Called by mode-selector cases 0 and 1. Record:
`functions/0x09d54-uni_mode_eth_10gbase_r_cfg.md`.

### 0x09f54 uni_mode_eth_5gbase_r_cfg
Status: complete. Internal Uni 5GBASE-R profile writer. It logs then performs
49 unconditional ordered 32-bit stores at `uni_serdes_base + 0x00..0xc0` with
no CPU predicate, read, or RMW. Called by mode-selector cases 2 and 3. Record:
`functions/0x09f54-uni_mode_eth_5gbase_r_cfg.md`.

### 0x0a154 uni_mode_eth_2p5gbase_r_cfg
Status: complete. Internal Uni 2.5GBASE-R profile writer. It logs then performs
49 unconditional ordered 32-bit stores at `uni_serdes_base + 0x00..0xc0` with
no CPU predicate, read, or RMW. Called by mode-selector case 4. Record:
`functions/0x0a154-uni_mode_eth_2p5gbase_r_cfg.md`.

### 0x0a354 uni_mode_eth_2p5gbase_x_cfg
Status: complete. Internal Uni 2.5GBASE-X profile selector. CPU 133 has a
priority profile; otherwise CPU 129 has a distinct profile; unsupported CPUs
only log. Each supported profile performs 49 ordered 32-bit stores. Called by
mode-selector cases 5 and 6. Record:
`functions/0x0a354-uni_mode_eth_2p5gbase_x_cfg.md`.

### 0x0a6fc uni_mode_eth_1gbase_x_cfg
Status: complete. Internal Uni 1GBASE-X profile selector. CPU 133 has a
priority profile; otherwise CPU 129 has a distinct profile; unsupported CPUs
only log. Each supported profile performs 49 ordered 32-bit stores. Called by
mode-selector cases 7 and 8. Record:
`functions/0x0a6fc-uni_mode_eth_1gbase_x_cfg.md`.

### 0x0aa90 uni_serdes_mode_set
Status: complete. Internal Uni mode dispatcher. Modes 0-8 select the five
recovered Ethernet profiles in fixed groups; all other unsigned modes do
nothing. Called by `uni_zx_serdes_init`. Record:
`functions/0x0aa90-uni_serdes_mode_set.md`.

### 0x0aae8 uni_zx_serdes_init
Status: complete. Internal Uni SerDes bring-up: applies the profile dispatcher,
enables `+0x54[0]`, polls CPU/mode-specific PLL status, reports ALOS, waits for
CDR lock, and performs an extra CPU-129 status check. Returns zero or `-1` on
any timeout. Record: `functions/0x0aae8-uni_zx_serdes_init.md`.

### 0x0acb0 uni_pll_cfg
Status: complete. Internal Uni PLL CRM configuration. Modes 0-4 and 5-7 apply
distinct literal profiles with ordered CRM RMWs; every other mode is a no-op
and all paths return zero. Record: `functions/0x0acb0-uni_pll_cfg.md`.

### 0x0ada8 uni_eth_mode_change
Status: complete. Internal nine-entry unsigned mode mapper for Uni-to-PON clock
reset configuration. Unknown modes log and return zero; the sole caller only
acts on nonzero results. Record: `functions/0x0ada8-uni_eth_mode_change.md`.

### 0x0ae34 uni_serdes_init
Status: complete. Exported Uni SerDes/XMAC wrapper. XMAC zero pulses CRM reset
bits and invokes bring-up while swallowing its status; XMAC one maps modes into
PON clock resets only when allowed by the ownership flag. Always returns zero.
Record: `functions/0x0ae34-uni_serdes_init.md`.

### 0x0af44 uni_serdes_on_133
Status: complete. Internal helper that sets `uni_serdes_base + 0x54[0]` with a
single volatile RMW and returns the post-write word. It does not check CPU type.
Record: `functions/0x0af44-uni_serdes_on_133.md`.

### 0x0e004 net_tst_tx
Status: complete. Validates a nonempty input and a 0-3 CPU-netdev slot, allocates
and copies an skb, delegates ownership to `cpu_net_tx`, and returns `-1` only
for invalid input. Allocation failure increments the selected device TX-drop
word but returns zero. Record: `functions/0x0e004-net_tst_tx.md`.

### 0x0e0c0 oam_tx
Status: complete. Global OAM wrapper that calls `net_tst_tx(data, length, 2)`
and propagates its 32-bit status. No direct in-module callers. Record:
`functions/0x0e0c0-oam_tx.md`.

### 0x0e0d8 net_omci_tx_test
Status: complete. Global diagnostic management sender: allocates a byte ramp,
prints it, sends it through port 2, and frees the temporary buffer. Its residual
machine return is not semantic; recovered ABI is void. Record:
`functions/0x0e0d8-net_omci_tx_test.md`.

### 0x0e524 dump_net_int_info
Status: complete. Read-only diagnostic for one of four `int_info` slots. It
rejects unsigned indices above three, then prints five consecutive counters at
`+0x18c..+0x19c`; its semantic ABI is void. Record:
`functions/0x0e524-dump_net_int_info.md`.

### 0x0e5c8 sub_E5C8
Status: complete. Unnamed CPU-local PMR transaction: writes the input, barriers,
writes its readback XOR `0xe0`, reads `TPIDR_EL2`, then falls through into the
adjacent byte reversal. Purpose remains unknown. Record:
`functions/0x0e5c8-sub_E5C8.md`.

### 0x0e5e0 __fswab32
Status: complete. Distinct two-instruction `REV W0,W0` byte-reversal entry used
by GSO/TCP paths and as `sub_E5C8`'s physical fall-through target. Record:
`functions/0x0e5e0-fswab32.md`.

### 0x0e5e8 virt_to_phys
Status: complete. Local address conversion helper: a `vabits_actual`-based
predicate selects either `address - kimage_voffset` or masked-low-39-bits plus
`memstart_addr`. Record: `functions/0x0e5e8-virt_to_phys.md`.

### 0x0fa60 sub_FA60
Status: complete. Two PAN writes, then physical fall-through to
`testftp_net_report` with arguments unchanged. The PAN toggle purpose is
unknown. Record: `functions/0x0fa60-sub_FA60.md`.

### 0x0fa68 testftp_net_report
Status: complete. Increments a testftp counter, parses IPv4/IPv6-shaped packet
lengths from metadata-directed headers, optionally logs, and propagates an
external FFE preprocessor result. Record:
`functions/0x0fa68-testftp_net_report.md`.

### 0x0fb6c testftp_init
Status: complete. Stores literal one to `g_speedtesthffenable`; residual global
address is not a semantic return. Record: `functions/0x0fb6c-testftp_init.md`.

### 0x10414 buf_fifo_rls
Status: complete. Drains a selected per-CPU FIFO through its allocation helper
and selection-specific release helper, up to signed `mask + 1` attempts. Record:
`functions/0x10414-buf_fifo_rls.md`.

### 0x104c8 buf_fifo_rls_all
Status: complete. Calls the FIFO drain action with selections 0 through 3 in
fixed order; no direct callers. Record: `functions/0x104c8-buf_fifo_rls_all.md`.

### 0x104f8 idm_recycle_stats
Status: complete. Unsynchronized four-FIFO diagnostic printer for usage and
eleven raw counter fields; semantic ABI is void. Record:
`functions/0x104f8-idm_recycle_stats.md`.

### 0x1063c idm_recycle_init
Status: complete. Initializes selected per-CPU recycle words and four 4096-slot
FIFO pointer rings, retaining allocation failures without cleanup. Record:
`functions/0x1063c-idm_recycle_init.md`.

### 0x11520 __raw_readl
Status: complete. Raw volatile 32-bit load; the adjacent `0x11a0c` item is ARM64
alternative-instruction data, not a function. Record: `functions/0x11520-raw_readl.md`.

### 0x11528 timer_refresh_config_load_reg
Status: complete. Reads timer `+0x10`, XORs with `0x0f`, and writes it back.
Record: `functions/0x11528-timer_refresh_config_load_reg.md`.

### 0x1154c timer0_process
Status: complete. High-priority tasklet action invokes optional callback,
re-enables the global Timer0 IRQ, and increments its counter. Record:
`functions/0x1154c-timer0_process.md`.

### 0x1158c timer_int_handler
Status: complete. Masks the Timer0 IRQ, atomically schedules the tasklet once,
and returns handled. Record: `functions/0x1158c-timer_int_handler.md`.

### 0x11614 timer0_config
Status: complete. Nonzero-rate Timer0 25 MHz divisor transaction. Record:
`functions/0x11614-timer0_config.md`.

### 0x11664 timer0_start
Status: complete. Writes one to mapped Timer0 `+0x0c`. Record:
`functions/0x11664-timer0_start.md`.

### 0x11680 timer0_stop
Status: complete. Writes zero to mapped Timer0 `+0x0c`. Record:
`functions/0x11680-timer0_stop.md`.

### 0x11698 zx_timer0_stop
Status: complete. Exported `timer0_stop` wrapper. Record:
`functions/0x11698-zx_timer0_stop.md`.

### 0x116ac timer1_init
Status: complete. Programs Timer1 reload/config/refresh/barrier/enable sequence.
Record: `functions/0x116ac-timer1_init.md`.

### 0x116fc timer1_get_counter
Status: complete. Returns zero when unmapped or raw Timer1 `+0x18` otherwise.
Record: `functions/0x116fc-timer1_get_counter.md`.

### 0x11728 timer0_register_func
Status: complete. Stores and returns Timer0 callback pointer. Record:
`functions/0x11728-timer0_register_func.md`.

### 0x11734 zx_timer_wclk_sel
Status: complete. Replaces LSP0 CRM bit nine at two offsets from input bit zero.
Record: `functions/0x11734-zx_timer_wclk_sel.md`.

### 0x11794 zx_timer_init
Status: complete. Maps vendor Timer0/Timer1/LSP nodes, configures clock selection,
and requests Timer0 IRQ without cleanup. Record: `functions/0x11794-zx_timer_init.md`.

### 0x1193c timer0_config_dothz
Status: complete. Nonzero-rate Timer0 250 MHz divisor transaction. Record:
`functions/0x1193c-timer0_config_dothz.md`.

### 0x1198c zx_timer0_start
Status: complete. Exported Timer0 initialization/configuration/affinity/start
wrapper. Record: `functions/0x1198c-zx_timer0_start.md`.

### 0x11a10 soam_init
Status: complete. Enables two NPPT control bits, polls ready bit one indefinitely,
logs, and returns zero. Record: `functions/0x11a10-soam_init.md`.

### 0x11aac nppt_nppu_reset
Status: complete. Pulses NPPT control-word bit seven with one fixed delay and
returns zero. Record: `functions/0x11aac-nppt_nppu_reset.md`.

### 0x11b3c nppt_tm_reset
Status: complete. Pulses the same NPPT control word's bit eight with one fixed
delay and returns zero. Record: `functions/0x11b3c-nppt_tm_reset.md`.

### 0x11bcc nppt_exit
Status: complete. Calls `idm_exit` then `smac_del_phy_scan`; module cleanup
invokes it before platform-driver unregistration. Record: `functions/0x11bcc-nppt_exit.md`.

### 0x11be4 arch_local_irq_save_0
Status: complete. Reads full DAIF and conditionally masks local IRQs. Record:
`functions/0x11be4-arch_local_irq_save_0.md`.

### 0x11bfc arch_local_irq_restore_1
Status: complete. Full DAIF restore paired with the GREG auto-gate callers.
Record: `functions/0x11bfc-arch_local_irq_restore_1.md`.

### 0x11c08 greg_sdet_to_reset
Status: complete. Clears NPPT SDET control bit four and logs the transition.
Record: `functions/0x11c08-greg_sdet_to_reset.md`.

### 0x11c64 greg_init_done_check
Status: complete. Polls NPPT ready mask `0x1fd` for 400 delayed retries. Record:
`functions/0x11c64-greg_init_done_check.md`.

### 0x11ce8 greg_sdet_to_restore
Status: complete. Delays then sets SDET control bit four in a cached word.
Record: `functions/0x11ce8-greg_sdet_to_restore.md`.

### 0x11d54 do_raw_spin_lock_flags.isra.1.constprop.3
Status: complete. Hard-wired raw lock acquisition for NPPT auto-gate state.
Record: `functions/0x11d54-raw_spin_lock_flags_isra_1.md`.

### 0x11d98 greg_sopc_auto_gate_en_get
Status: complete. DAIF/lock-protected read of NPPT `+0xb8` bit four. Record:
`functions/0x11d98-greg_sopc_auto_gate_en_get.md`.

### 0x11df8 greg_sopc_auto_gate_en_set
Status: complete. DAIF/lock-protected replacement of NPPT `+0xb8` bit four.
Record: `functions/0x11df8-greg_sopc_auto_gate_en_set.md`.

### 0x11e60 greg_smac0_3_mask_runt_err
Status: complete. Unchecked indexed SMAC runt-mask RMW. Record:
`functions/0x11e60-greg_smac0_3_mask_runt_err.md`.

### 0x11e80 greg_smac6_mask_runt_err
Status: complete. Sets NPPT `+0x4c` bit 16. Record:
`functions/0x11e80-greg_smac6_mask_runt_err.md`.

### 0x11e98 greg_xmac_mask_runt_type
Status: complete. Unchecked indexed XMAC runt-type RMW. Record:
`functions/0x11e98-greg_xmac_mask_runt_type.md`.

### 0x11ebc greg_smac_mask_runt_err
Status: complete. Fixed seven-call SMAC/XMAC runt-mask wrapper. Record:
`functions/0x11ebc-greg_smac_mask_runt_err.md`.

### 0x11f00 greg_init
Status: complete. Programs BP sizes/usable length, masks runt errors, returns
zero. Record: `functions/0x11f00-greg_init.md`.

### 0x11f60 greg_rgmii_intf_mode_set
Status: complete. Controls NPPT `+0x30` bits 17-18 from exact zero mode. Record:
`functions/0x11f60-greg_rgmii_intf_mode_set.md`.

### 0x11f84 greg_sdet_share_clk_cfg
Status: complete. Validates enable 0/1, replaces NPPT `+0x19c` bit zero, and
returns enable or `-1`. Record: `functions/0x11f84-greg_sdet_share_clk_cfg.md`.

### 0x1293c smac_del_phy_scan
Status: complete. Calls only `del_timer(&phy_timer)` during NPPT exit. Record:
`functions/0x1293c-smac_del_phy_scan.md`.

### 0x12980 nppt_smac_set_rgmii_mode
Status: complete. Programs one NPPT RGMII field, applies active-low CRM clock
selection and GREG RGMII mode one, then logs. Record:
`functions/0x12980-nppt_smac_set_rgmii_mode.md`.

### 0x12f88 test_and_set_bit
Status: complete. 64-bit atomic bit-set primitive returning the previous bit
state; `_check_abuf` is its only direct caller. Record:
`functions/0x12f88-test_and_set_bit.md`.

### 0x13008 __fswab32_1
Status: complete. IDM-local 32-bit byte reversal clone. Record:
`functions/0x13008-fswab32_1.md`.

### 0x13010 virt_to_phys_0
Status: complete. IDM-local 64-bit virtual-to-physical arithmetic helper. Record:
`functions/0x13010-virt_to_phys_0.md`.

### 0x1305c __my_cpu_offset_0
Status: complete. TPIDR_EL1 accessor used for IDM per-CPU bases. Record:
`functions/0x1305c-my_cpu_offset_0.md`.

### 0x13064 arch_local_irq_save_1
Status: complete. DAIF save/local-IRQ mask clone used by IDM interrupt masking.
Record: `functions/0x13064-arch_local_irq_save_1.md`.

### 0x1307c arch_local_irq_restore_2
Status: complete. DAIF restore clone paired with IDM interrupt masking. Record:
`functions/0x1307c-arch_local_irq_restore_2.md`.

### 0x130c8 idm_rx_update
Status: complete. Ops-table RX publication callback with `DSB ST` and two packed
IDM count/queue MMIO writes. Record: `functions/0x130c8-idm_rx_update.md`.

### 0x130f0 idm_rx_test
Status: complete. Exported constant-zero RX test stub. Record:
`functions/0x130f0-idm_rx_test.md`.

### 0x130f8 idm_recv_debug_set
Status: complete. Exported no-op compatibility stub. Record:
`functions/0x130f8-idm_recv_debug_set.md`.

### 0x130fc idm_tx_debug_set
Status: complete. Exported `net_tx_debug` setter. Record:
`functions/0x130fc-idm_tx_debug_set.md`.

### 0x13108 idm_rx_debug_set
Status: complete. Exported `net_rx_debug` setter. Record:
`functions/0x13108-idm_rx_debug_set.md`.

### 0x13114 idm_wifi_tx_debug_set
Status: complete. Exported `idm_tx_debug` setter. Record:
`functions/0x13114-idm_wifi_tx_debug_set.md`.

### 0x13120 idm_wifi_rx_debug_set
Status: complete. Exported paired `idm_rx_debug`/`np1_trap_debug` setter. Record:
`functions/0x13120-idm_wifi_rx_debug_set.md`.

### 0x13134 idm_omci_tx_debug_set
Status: complete. Exported `omci_tx_debug` setter. Record:
`functions/0x13134-idm_omci_tx_debug_set.md`.

### 0x13140 idm_omci_rx_debug_set
Status: complete. Exported `omci_rx_debug` setter. Record:
`functions/0x13140-idm_omci_rx_debug_set.md`.

### 0x1314c idm_set_smct_all_trap
Status: complete. Exported IDM bit-14 RMW control with literal-zero status
return. Record: `functions/0x1314c-idm_set_smct_all_trap.md`.

### 0x13174 set_last_extral_cnt
Status: complete. Exported raw `last_extral_cnt` setter. Record:
`functions/0x13174-set_last_extral_cnt.md`.

### 0x13184 set_last_normal_cnt
Status: complete. Exported raw `last_normal_cnt` setter. Record:
`functions/0x13184-set_last_normal_cnt.md`.

### 0x13194 set_last_jumbo_cnt
Status: complete. Exported raw `last_jumbo_cnt` setter. Record:
`functions/0x13194-set_last_jumbo_cnt.md`.

### 0x131a4 get_last_buffer_idx
Status: complete. Exported three-pool last-index query with invalid-selector
zero return. Record: `functions/0x131a4-get_last_buffer_idx.md`.

### 0x131f0 set_last_buffer_idx
Status: complete. Exported three-pool last-index setter with invalid-selector
no-op. Record: `functions/0x131f0-set_last_buffer_idx.md`.

### 0x13234 idm_stat
Status: complete. Exported 35-line IDM counter/debug diagnostic sampler; paired
16-bit counters are read once and printed high then low. Record:
`functions/0x13234-idm_stat.md`.

### 0x13568 idm_debug_stat
Status: complete. Exported fixed-two-CPU software buffer-pool counter dump that
prints repeat counters then invokes `idm_stat`. Record:
`functions/0x13568-idm_debug_stat.md`.

### 0x136bc idm_print_bppe
Status: complete. Exported one-word hexadecimal diagnostic print helper. Record:
`functions/0x136bc-idm_print_bppe.md`.

### 0x136dc data_padding
Status: complete. Conditional raw skb tail zero-fill followed by a fixed
60-byte-length store. Record: `functions/0x136dc-data_padding.md`.

### 0x13740 idm_rls_update
Status: complete. CPU 129/133-gated reorder-release ops callback with two MMIO
writes and deliberately repeated type probe. Record:
`functions/0x13740-idm_rls_update.md`.

### 0x137c4 idm_cpu_nb_tx_update
Status: complete. DSB-protected queue/count TX publication callback. Record:
`functions/0x137c4-idm_cpu_nb_tx_update.md`.

### 0x13898 idm_get_smct_all_trap
Status: complete. Exported checked getter for IDM base bit 14. Record:
`functions/0x13898-idm_get_smct_all_trap.md`.

### 0x1397c get_order
Status: complete. ARM64 4 KiB page-order calculation with unsigned size-zero
wraparound behavior. Record: `functions/0x1397c-get_order.md`.

### 0x139a4 set_idm_int_cpu_rx_cpu_config
Status: complete. Exported two-CPU IRQ0 affinity selector that synchronizes
`g_idm_irq_to_cpu` and `eth_xmit_mode`. Record:
`functions/0x139a4-set_idm_int_cpu_rx_cpu_config.md`.

### 0x13a2c do_raw_spin_lock_flags.isra.1.constprop.21
Status: complete. Fixed `idm_lock_int` exclusive qspinlock acquisition helper
used by IDM interrupt-mask updates. Record:
`functions/0x13a2c-do_raw_spin_lock_flags-isra-1-constprop-21.md`.

### 0x13bd8 do_raw_spin_lock_1
Status: complete. Generic pointer-based exclusive qspinlock acquisition helper.
Record: `functions/0x13bd8-do_raw_spin_lock_1.md`.

### 0x13c14 idm_rx_refill_flush
Status: complete. Locks and drains current-CPU normal/jumbo staging to RX rings,
preserving the jumbo normal-bound wrap. Record:
`functions/0x13c14-idm_rx_refill_flush.md`.

### 0x13d4c idm_rx_refill_reuse
Status: complete. Requeues a byte-swapped old buffer into the selected RX ring
after a locked index advance. Record:
`functions/0x13d4c-idm_rx_refill_reuse.md`.

### 0x13df8 idm_alloc_buf
Status: complete. Normal per-CPU FIFO-batch/stash allocator with generic FIFO,
kmem, and slab fallbacks. Record: `functions/0x13df8-idm_alloc_buf.md`.

### 0x14034 idm_alloc_nbuf
Status: complete. IDM ops normal-buffer allocator that initializes raw header
fields and updates a per-CPU status lane. Record:
`functions/0x14034-idm_alloc_nbuf.md`.

### 0x142b0 idm_fifo_in
Status: complete. Softirq-bracketed FIFO producer with checked full condition
and rate-limited failure diagnostic. Record: `functions/0x142b0-idm_fifo_in.md`.

### 0x143c4 idm_free_buf
Status: complete. IDM ops buffer-release callback with boundary-based direct
free/FIFO return and normal per-CPU batch stash path. Record:
`functions/0x143c4-idm_free_buf.md`.

### 0x14604 idm_free_skb_data
Status: complete. Installed skb-data release callback with raw flag-driven
primary/auxiliary backing-buffer ownership paths. Record:
`functions/0x14604-idm_free_skb_data.md`.

### 0x148b4 dump_tx_desc
Status: complete. Two-line raw TX descriptor diagnostic printer used by seven
TX/debug paths. Record: `functions/0x148b4-dump_tx_desc.md`.

### 0x14b8c dump_tx_desc_wifi
Status: complete. Reduced Wi-Fi TX descriptor diagnostic printer. Record:
`functions/0x14b8c-dump_tx_desc_wifi.md`.

### 0x14cd4 idm_check_all_tx_desc
Status: complete. Unused diagnostic checker that repeatedly validates only the
selected TX ring's first descriptor against the live queue depth. Record:
`functions/0x14cd4-idm_check_all_tx_desc.md`.

### 0x15e94 idm_exit
Status: complete. Empty IDM cleanup hook called by `nppt_exit`. Record:
`functions/0x15e94-idm_exit.md`.

### 0x15e98 _check_abuf
Status: complete. Private normal/jumbo BPPE diagnostic that marks buffers found
in a FIFO, per-CPU stashes, and refill rings, then reports duplicates and
missing entries. Record: `functions/0x15e98-check_abuf.md`.

### 0x16574 idm_check_bppe
Status: complete. Exported byte-selector forwarding wrapper for the BPPE
diagnostic backend. Record: `functions/0x16574-idm_check_bppe.md`.

### 0x16588 check_bppe
Status: complete. Private normal-pool BPPE diagnostic wrapper. Record:
`functions/0x16588-check_bppe.md`.

### 0x165a0 sub_165A0
Status: complete. Unused local system-register prefix which falls through into
`sipc_init`. Record: `functions/0x165a0-sub_165A0.md`.

### 0x165b8 sipc_init
Status: complete. NPPT `+0x4000` constant-zero initializer. Record:
`functions/0x165b8-sipc_init.md`.

### 0x165d0 xmac_eee_conf
Status: complete. Exported two-word XMAC EEE control/timing programmer with
special selector-2/3 address windows. Record:
`functions/0x165d0-xmac_eee_conf.md`.

### 0x1677c xmac0_wan_port_sel
Status: complete. Exported 32-bit system-control write of a byte port selector.
Record: `functions/0x1677c-xmac0_wan_port_sel.md`.

### 0x16790 xmac_status_show
Status: complete. Exported two-XMAC global mode/auto-negotiation diagnostic with
unchecked local label-table indexing. Record:
`functions/0x16790-xmac_status_show.md`.

### 0x1693c phy_dynamic_identify
Status: complete. Local ordered external-PHY symbol probe that updates the PHY
type selector only when a matching API exists. Record:
`functions/0x1693c-phy_dynamic_identify.md`.

### 0x169a0 xmac_get_nppt_glb_xpcs_speed_duplex_in_sgmii_mode
Status: complete. Unchecked three-output extraction from one per-XMAC NPPT
status word. Record:
`functions/0x169a0-xmac_get_nppt_glb_xpcs_speed_duplex_in_sgmii_mode.md`.

### 0x169d0 xmac_get_nppt_glb_xpcs_speed_duplex_in_usxgmii_mode
Status: complete. Parameter-forwarding wrapper for the SGMII-named NPPT status
reader. Record:
`functions/0x169d0-xmac_get_nppt_glb_xpcs_speed_duplex_in_usxgmii_mode.md`.

### 0x17034 xamc_init_conf
Status: complete. Local fixed five-word XMAC configuration programmer without
the speed/duplex setup performed by its related helper. Record:
`functions/0x17034-xamc_init_conf.md`.

### 0x17a50 xmac_test_siwtch_work_mode
Status: complete. Exported ten-mode test dispatcher that enables TX/RX after
every valid child configuration result. Record:
`functions/0x17a50-xmac_test_siwtch_work_mode.md`.

### 0x18240 xmac_work_mode_switch_to_serdes_mode
Status: complete. Local XMAC-mode to SerDes-mode mapper with an invalid-mode
diagnostic and `-1` result. Record:
`functions/0x18240-xmac_work_mode_switch_to_serdes_mode.md`.

### 0x182c4 xmac_rlt_phy_init
Status: complete. Local CPU-variant gating stub with no observed PHY operation
or semantic return value. Record: `functions/0x182c4-xmac_rlt_phy_init.md`.

### 0x182e4 xmac_mvl_phy_init
Status: complete. Local CPU/XMAC-type gating stub with no observed Marvell PHY
operation or semantic return value. Record:
`functions/0x182e4-xmac_mvl_phy_init.md`.

### 0x18324 xmac_bcm_phy_init
Status: complete. Local one-instruction empty Broadcom-PHY-named stub. Record:
`functions/0x18324-xmac_bcm_phy_init.md`.

### 0x18328 xmac_aqr_phy_init
Status: complete. Local CPU-variant gating stub with no observed AQR PHY
operation or semantic return value. Record:
`functions/0x18328-xmac_aqr_phy_init.md`.

### 0x18348 xmac_zxic_phy_init
Status: complete. Conditionally initializes ZXIC PHY slots and registers eight
same-index callback-table entries for each present PHY. Record:
`functions/0x18348-xmac_zxic_phy_init.md`.

### 0x1845c xmac_jl_phy_init
Status: complete. Local one-instruction empty JL-PHY-named stub. Record:
`functions/0x1845c-xmac_jl_phy_init.md`.

### 0x00128 pon_set_pll_pon_ref_clock
Status: complete. Replaces PON CRM offset `0x10` bits 4-5 from the low two input
bits. Its sole caller is the PON PLL reference selector. Record:
`functions/0x00128-pon_set_pll_pon_ref_clock.md`.

### 0x0014c pon_set_pll_pon_cfg_with_ref_clk_25M
Status: complete. Writes a fixed PON PLL CRM profile at offsets `0xc0`-`0xcc`,
then sets bit 28. Its sole caller is the PON PLL reference selector. Record:
`functions/0x0014c-pon_set_pll_pon_cfg_with_ref_clk_25M.md`.

### 0x00188 pon_set_pll_pon_en
Status: complete. Replaces PON CRM offset `0xc4` bit 28 from input bit zero.
No direct code xrefs; recovered `(u8) -> int` ABI. Record:
`functions/0x00188-pon_set_pll_pon_en.md`.

### 0x001ac pon_use_pll_pon_ref_from_ex_pll
Status: complete. Fixed-order PON PLL reference setup transaction that writes
three literal-one selectors then a 25 MHz PLL profile. No direct code xrefs.
Record: `functions/0x001ac-pon_use_pll_pon_ref_from_ex_pll.md`.
