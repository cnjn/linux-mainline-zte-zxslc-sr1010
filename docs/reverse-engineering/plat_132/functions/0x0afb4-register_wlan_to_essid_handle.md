# 0x0afb4 register_wlan_to_essid_handle

## Status

- Status: complete
- Confidence: verified private-slot store, residual return, export, and the
  captured `np.ko` registration caller; callback ABI is only partly evidenced.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature: `void *register_wlan_to_essid_handle(void *callback)`.

## Semantics

Stores the raw callback pointer in `idm_wlanname_to_essid` and returns it
unchanged. Null clears the slot. It has no validation, synchronization,
reference/lifetime management, or previous-slot return.

## Caller Context

There are no direct in-module callers. The API is exported, and `np.ko` imports
it: `tm_initial @ 0xed3f4` passes its `aclDevNameToWlanIDMMap @ 0xe12d8`
function pointer. That provider forwards its first input to `IfName2WlanIdmMap`,
writes byte outputs through its next three pointer arguments, and returns zero
or `-1`; it provides a candidate ABI, not a proven `plat_132` consumer contract.
`idm_wlanname_to_essid` has no other resolved in-module references.

## Concurrency and Ownership

- Plain unsynchronized raw-pointer publication.
- The external callback provider retains lifetime and synchronization
  responsibility.

## Evidence

- Complete three-instruction ARM64 body at `0xafb4` through `0xafbc`.
- Store target is private global `idm_wlanname_to_essid @ 0x27948`.
- No direct IDA callers or other resolved accesses to the slot.
- Runtime kallsyms exports the API; `np.ko` undefined-symbol and relocation
  evidence identify its initialization-time registration call.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact callback ABI, callback consumer, and why this `plat_132` build retains
  the private slot without a resolved in-module use.
