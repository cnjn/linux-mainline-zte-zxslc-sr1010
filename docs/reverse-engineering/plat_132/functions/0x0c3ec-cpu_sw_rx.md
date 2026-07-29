# 0x0c3ec cpu_sw_rx

## Status

- Status: complete
- Confidence: verified predicate, callback branches, skb netdev overwrite,
  metadata call arguments, counter, and return behavior.
- Size: `0xc4` bytes, 49 ARM64 instructions.
- Recovered signature:
  `int cpu_sw_rx(struct sk_buff *skb, struct net_device *device, void *desc, void *meta, const void *data, u32 queue)`.

## Semantics

Only `skb`, descriptor, and queue arguments affect behavior. The function tests
the raw predicate:

```c
(((u8)descriptor[6] + 0x30) & 0x3f) <= 0x29
```

If true and `idm_skb_recv` is non-null, it increments raw counter `0x28140`,
overwrites `skb + 0x10` with the IDM netdev, reserves 40 bytes of scratch,
builds the helper's 36-byte trap record via
`idm_set_wifi_trap_info(descriptor, scratch, queue)`, and invokes
`idm_skb_recv(scratch, skb)`.

Otherwise it calls `switch_skb_recv(skb)` with no local null check. Its only
in-module caller, `cpu_net_rx`, checks `switch_skb_recv` before entering this
function. It always returns zero.

## Evidence

- Complete 49-instruction ARM64 disassembly at `0xc3ec` through `0xc4ac`.
- Sole caller `cpu_net_rx @ 0xc5dc` at `0xc9d4`.
- Direct `idm_set_wifi_trap_info @ 0xbd8c` decompilation confirms descriptor,
  output-record, and queue arguments.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The semantic classification encoded by the transformed descriptor byte is
  unknown; the Wi-Fi-trap routing label is based on the called metadata helper.
- Exact ownership contract of `idm_skb_recv` and `switch_skb_recv` remains in
  their registration paths.
