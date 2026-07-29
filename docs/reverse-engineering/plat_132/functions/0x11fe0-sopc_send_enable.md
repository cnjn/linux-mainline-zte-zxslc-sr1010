# 0x11fe0 sopc_send_enable

## Status

- Status: complete
- Confidence: verified selector split, normal/MAC6 poll limits, all register
  formulas, delay/log/write order, no-op path, void return, and all callers.
- Size: `0x198` bytes, 99 ARM64 instructions.
- Recovered signature: `void sopc_send_enable(u8 mac)`.

## Semantics

The function distinguishes three paths after byte-truncating `mac`:

- For `mac <= g_smac_max_index`, it polls up to ten times at
  `nppt_base + 0x34000 + 4 * ((mac + 0xa5) & 0x1ff)`. Each read is logged; an
  unset bit zero incurs a `0x418958` delay before the next try. On a ready bit,
  it delays, writes one at index `mac + 0xac`, and logs that early write. After
  either success or exhausted attempts it unconditionally writes one to the
  same send-enable register, so success writes it twice and timeout writes it
  once without the write diagnostic.
- For `mac > g_smac_max_index` and `mac != 6`, it returns without a side effect.
- For `mac == 6`, each of at most ten attempts first performs 100 fixed delays,
  then reads `nppt_base + 0x342ac` and logs the zero-based attempt index. A
  ready bit causes one more delay, a write of one at `nppt_base + 0x342c8`, a
  log, and return. A MAC6 timeout does not write the send-enable register.

## Caller Context

Direct calls occur through `sub_11FCC @ 0x11fcc`, from
`nppt_smac_config_speed_duplex @ 0x123ec`, and during
`nppt_smac_init @ 0x129c8`. All use it as a send-enable operation.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs volatile
MMIO-style polling and writes; bounded wait time differs between normal SMAC and
MAC6 paths.

## Evidence

- Complete 99-instruction ARM64 body at `0x11fe0` through `0x12174`.
- Exact `g_smac_max_index` routing, normal ready/send indexes `+0xa5`/`+0xac`,
  MAC6 fixed offsets `0x2ac`/`0x2c8`, retry limits, and 100-delay inner loop.
- Exact fixed delay constant `0x418958`, duplicated normal success write, and
  all diagnostics.
- Three direct caller xrefs.
- IDA function type updated at `0x11fe0` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware differences that require MAC6's distinct timing/register path.
- Whether a normal-path timeout followed by a forced send-enable write is safe.
