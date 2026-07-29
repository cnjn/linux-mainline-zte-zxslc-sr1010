# 0x19448 xpcs_auto_negotiation_conf_in_usxgmii_mode

## Status

- Status: complete
- Confidence: verified both byte inputs, selector rejection, call order,
  flag-one global update, return values, and all direct callers.
- Size: `0x68` bytes, 25 ARM64 instructions.
- Recovered signature:
  `int xpcs_auto_negotiation_conf_in_usxgmii_mode(u8 xmac, u8 auto_enable)`.

## Semantics

The helper truncates both inputs to bytes. A selector above four logs
`xmac_index(%d) is error` and returns `-1`. Valid selectors execute the
following order:

1. `xpcs_set_vr_mii_an_ctrl_an_intr_en(xmac, auto_enable)`.
2. `xpcs_set_sr_mii_ctrl_an_enable(xmac, auto_enable)`.
3. When the byte input equals exactly one, write one to
   `g_xmac_work_in_auto[xmac]`.
4. Return zero.

It does not clear `g_xmac_work_in_auto[xmac]` for a zero or non-one input.
The low-level PCS helpers consume bit zero of the input, so an odd non-one
input can set the PCS bits without taking the global-byte branch.

## Caller Context

Seven direct calls come from `nppt_smac_init` and the 10G, 5G, and 2.5G
USXGMII auto configuration functions. These callers OR its status into their
larger configuration status or discard it during initialization.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The shared auto-mode
byte is written without synchronization.

## Evidence

- Complete 25-instruction ARM64 body at `0x19448` through `0x194ac`.
- `UXTB W5,W0` and `UXTB W6,W1` establish both byte arguments.
- Selector check, exact two PCS calls, exact equality-to-one test, and byte
  store at `0x194a4`.
- Exhaustive direct xref query found seven caller sites.
- IDA type at `0x19448` updated to the recovered two-argument `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Why the vendor only updates the software auto-mode byte for exact input one.
