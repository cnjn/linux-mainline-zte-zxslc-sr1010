# plat_132 Reconstruction Completeness

## Artifact

- Target: `vendor-reference/sr1010-vendor-runtime/modules/files/kmodule/plat_132.ko`.
- SHA-256: `6cb3e7c9567b4549b5d1b1c6d87f502d3b07fcde0461c2d28a5ad115661d4d51`.
- Architecture: ARM64 Linux 5.4.196 relocatable module.
- IDA session: `plat132-analysis`.

## Coverage

The initial IDA survey contains 656 entries: 561 entries in the module's
internal text range and 95 imported-function veneers. The 561 internal entries
reconcile to 559 independent function records under `functions/`:

- `0x11a0c` is ARM64 `.altinstructions` data, not a callable function.
- `0x16d30` is a tail-dispatch entry belonging to
  `xmac_get_uni_speed_from_xmac`, not a standalone function.

Every independent internal function has a ledger entry, a per-function evidence
record, and a source-like implementation or an explicit statement that it is a
stub, tail entry, or data item. The imported veneers are ABI dependencies, not
vendor implementation to reconstruct.

The source-like representation is organized in nine independent C units:

- `recovered/plat_cpu_net.c`
- `recovered/plat_cpu_tx.c`
- `recovered/plat_idm.c`
- `recovered/plat_irq.c`
- `recovered/plat_module.c`
- `recovered/plat_nppt.c`
- `recovered/plat_probe.c`
- `recovered/plat_smac.c`
- `recovered/plat_timer.c`

`CALLBACK_INTERFACES.md` records exported callback publication, direct hook
slots, PHY callback tables, and observed companion-module symbol dependencies.

## Confidence Boundary

Control flow, argument registers, return behavior, raw loads/stores, constants,
and direct xrefs are treated as verified when documented from disassembly.
Semantic names for opaque fields, hardware register meanings, and some vendor
structure layouts are strong inferences when established by repeated usage.

This project intentionally does not claim recovery of original identifiers,
hardware register names, or omitted behavior where the module lacks evidence.

## Build Status

Each recovered C unit passes independent C11 syntax checking with
`-Wall -Wextra`. This verifies the consistency of the source-like
reconstruction; it is not a linked, loadable replacement module.

The remaining build blockers are:

- Vendor kernel-private type and API declarations are unavailable.
- Descriptor bit meanings, MMIO register names, and several structure layouts
  remain only raw-offset representations.
- Companion modules own callback writers, lifecycle policy, and some PHY/switch
  behavior outside `plat_132`.
- The captured hardware configuration and vendor kernel build environment are
  not reconstructed.

## Validation

- Target hash matches the recorded artifact hash.
- All nine recovered C units pass `cc -std=c11 -Wall -Wextra -fsyntax-only`.
- `git diff --check -- docs/reverse-engineering/plat_132` passes.
- No trailing whitespace is reported beneath `docs/reverse-engineering/plat_132`.
- The IDA database was saved after the final function type and comment updates.

## Recommended Follow-Up

Cross-check high-impact reconstructed flows against captured runtime dmesg,
interrupt counters, and interface state, then resolve only hardware/documentation
gaps that affect an intended consumer of the recovered representation.
