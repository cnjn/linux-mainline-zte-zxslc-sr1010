# 0x196cc xpcs_auto_negotiation_conf_in_sgmii_mode

## Status

- Status: complete
- Confidence: verified both byte arguments, selector/mode rejection, call
  order, flag-one branch, global write, return values, and both direct callers.
- Size: `0xac` bytes, 42 ARM64 instructions.
- Recovered signature:
  `int xpcs_auto_negotiation_conf_in_sgmii_mode(u8 xmac, u8 auto_enable)`.

## Semantics

The helper truncates both inputs to bytes. It prints the vendor error message
and returns `-1` when `xmac > 4`. For an in-range selector, it reads
`sg_xpcs_mode[xmac]`; a value other than three also returns `-1` without a log.

For mode three it executes this exact sequence:

1. Clear SR-MII AN enable.
2. Set the VR-MII AN interrupt control from `auto_enable`.
3. Set the VR-MII MAC-auto-switch control from `auto_enable`.
4. If `auto_enable == 1`, re-enable SR-MII AN and write byte one to
   `g_xmac_work_in_auto[xmac]`.
5. Otherwise, write literal one to the SGMII link-status control.
6. Return zero.

The non-one branch does not clear `g_xmac_work_in_auto[xmac]`; this is an
observed omission, not normalized by the reconstruction.

## Caller Context

`xmac_sgmii_conf @ 0x16ee4` calls it at `0x16f64` and `0x16fcc` in its two
CPU-dependent setup sequences. The caller ORs this helper's status into the
PCS configuration status before final XMAC configuration.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It writes the
shared auto-mode byte without synchronization.

## Evidence

- Complete 42-instruction ARM64 body at `0x196cc` through `0x19774`.
- `UXTB W5,W0` and saved `UXTB W6,W1` establish the two-byte-argument ABI.
- Selector gate, `sg_xpcs_mode[xmac] == 3` gate, and exact `-1` returns.
- All five call sites and the conditional global byte store at `0x19758`.
- Exhaustive direct xref query found two calls, both in `xmac_sgmii_conf`.
- IDA type at `0x196cc` updated to the recovered two-argument `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware semantics of the delegated AN-interrupt and MAC-auto-switch
  helpers.
- Whether retaining the auto-mode global on a non-one `auto_enable` input is
  intended vendor policy or a latent state bug.
