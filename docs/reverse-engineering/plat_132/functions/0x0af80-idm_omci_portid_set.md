# 0x0af80 idm_omci_portid_set

## Status

- Status: complete
- Confidence: verified exact empty body, no direct callers, and export.
- Size: `0x04` bytes, one ARM64 instruction.
- Recovered signature: `void idm_omci_portid_set(void)`.

## Semantics

The entire function is `RET`. It reads no argument or global, writes no state,
`local_omci_port_id` despite its exported name.

## Caller Context

There are no direct in-module callers. The stub is exported as
`__ksymtab_idm_omci_portid_set`; external ABI callers may pass arguments, but
the machine code ignores all argument registers.

## Concurrency and Ownership

No synchronization, allocation, callback invocation, or ownership behavior.

## Evidence

- Complete single-instruction ARM64 body at `0xaf80`.
- Zero direct IDA xrefs and runtime kallsyms export.
- `local_omci_port_id` xrefs occur only in OMCI RX/TX consumers, not this stub.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Why a named exported port-ID setter is intentionally a no-op in this build.
