# 0x126e4 check_phy

## Status

- Status: complete
- Confidence: verified callback/state flow, changed-state branches, MAC/XMAC
  dispatch guards, helper arguments, cache update, caller set, and runtime link
  events; semantic void signature is a strong inference from residual returns.
- Size: `0x1ac` bytes, 107 ARM64 instructions.
- Recovered signature: `void check_phy(u8 mac)`.

## Semantics

The function reads byte `check_phy_en` and returns immediately only when it is
exactly one. It performs no bound check on `mac`. Otherwise it loads
`sg_smac_check_phy[mac]`; a null callback also returns immediately.

For a nonnull callback it calls:

```c
status = sg_smac_check_phy[mac](uni_phy[mac]);
```

Only a changed 32-bit `status` compared with `uni_phy_stat[mac]` triggers
side effects. After every changed state, including link down, it writes the raw
status back to `uni_phy_stat[mac]`.

`status == -1` is link down: it logs, calls `nppt_smac_disable(mac)`, and for
MAC 4 or 5 invokes the four-argument PCS helper with
`(mac - 4, 0, 3, 1)`.

All other changed values derive `speed = (u8)status` and
`duplex = ((u32)status >> 10) & 1`. Values with speed at most six emit the
speed/duplex diagnostic. Let `xmac_slot = (u8)(mac - 4)`:

- When `xmac_slot > 1`, it calls
  `nppt_smac_config_speed_duplex(mac, status, duplex)`.
- For slots zero or one, it calls `xmac_config_speed_duplex` only when CPU type
  is 133 or 129 and both guards hold:
  `(g_xmac0_type != 0 || mac != 4)` and
  `(g_xmac1_type != 0 || mac != 5)`.
- That XMAC path additionally calls the PCS helper with
  `(xmac_slot, 1, speed, 1)` when `duplex` is zero.

Every non-link-down changed state calls `nppt_smac_enable(mac)` after its
configuration decision. The machine code leaves different helper results in
X0 on different paths, but all known callers ignore them; semantic return type
is therefore void rather than a verified ABI declaration.

## Caller Context

`smac_check_phy_task_thread @ 0x12890` contains the seven direct call sites,
one each for MAC indexes zero through six, in every poll iteration.

## Concurrency and Ownership

Runs in the PHY worker context. `check_phy_en`, callback slots, state cache, and
XMAC type globals are read or written without local synchronization. Callback
and hardware helper contracts establish any required PHY/MMIO synchronization.
No allocation or ownership transfer occurs locally.

## Evidence

- Complete 107-instruction ARM64 body at `0x126e4` through `0x1288c`.
- Exact byte guard, callback invocation, signed `-1` test, cache comparison,
  speed/duplex extraction, and cache write.
- Raw four-register PCS calls on link down and half-duplex changes.
- Direct decompilation/disassembly of SMAC/XMAC configuration and enable/disable
  helpers, plus verified CPU and type-state predicates.
- Seven worker caller xrefs and runtime `mac 5 link down` / `1000M full-duplex`
  messages.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Full encoding of callback status values beyond low-byte speed and bit-10
  duplex observed here.
- Exact meaning/lifecycle of `check_phy_en` and XMAC type-state globals.
- Locking and lifetime requirements for externally installed PHY callbacks.
