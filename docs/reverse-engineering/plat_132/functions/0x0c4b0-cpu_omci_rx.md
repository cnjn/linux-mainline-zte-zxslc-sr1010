# 0x0c4b0 cpu_omci_rx

## Status

- Status: complete
- Confidence: verified callback/null behavior, work-mode branch, MIC check,
  metadata port access, port clamp, and return values.
- Size: `0x12c` bytes, 74 ARM64 instructions.
- Recovered signature:
  `int cpu_omci_rx(void *descriptor, void *metadata, const void *data, u32 length)`.

## Semantics

The function is called only from the management-descriptor branch of
`cpu_net_rx`. Debug dump logic aside, it behaves as follows:

```c
if (!omci_oam_rx)
    return 0;

port = *(u16 *)(metadata + 2);
if (g_pon_work_mode & 0xe40) {
    if (omci_mic_check && omci_mic_check(data, length))
        return -1;
    local_omci_port_id = port;
    omci_oam_rx(data, length, 0);
} else {
    if (port > 7)
        port = 0;
    omci_oam_rx(data, length, port);
}
return 0;
```

The callback's absence is not considered an error. In OMCI work modes the
metadata port is recorded but callback argument three is forced to zero. In OAM
work modes it is passed only when within the inclusive range 0 through 7.

## Caller Effect

`cpu_net_rx` interprets a nonzero result as a management-device drop. Therefore
only the MIC-check failure path makes this helper report failure; a missing
consumer callback still produces a successful management RX accounting path.

## Evidence

- Complete 74-instruction ARM64 disassembly at `0xc4b0` through `0xc5d8`.
- Sole caller `cpu_net_rx @ 0xc5dc` at `0xc7c4`.
- Direct register setup for callbacks and `metadata + 2` port load.
- Work-mode mask `0xe40` matches management-netdev name selection in
  `cpu_net_init`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original OMCI/OAM callback registration and MIC algorithm contracts remain
  outside this function.
- Descriptor/metadata type names remain analyst labels.
