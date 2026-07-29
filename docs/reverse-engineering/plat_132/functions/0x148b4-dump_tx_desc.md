# 0x148b4 dump_tx_desc

## Status

- Status: complete
- Confidence: verified both format strings, all raw descriptor reads/masks, void
  ABI, and seven direct callers.
- Size: `0x88` bytes, 34 ARM64 instructions.
- Recovered signature: `void dump_tx_desc(const void *descriptor)`.

## Semantics

Prints two diagnostic lines without modifying the descriptor. The first prints
the descriptor pointer and words `+0x00/+0x04/+0x08`, then raw fields:

| Field | Extraction |
| --- | --- |
| `p` | byte `+0x07 & 0x3f` |
| `l` | `(u16(+0x04) >> 1) & 0x3fff` |
| `out` | byte `+0x0a & 0x3f` |
| `gemport` | `u16(+0x18)` |
| `qid` | `u16(+0x1a) & 0x1ff` |
| `idm_flag` | `(byte(+0x1b) >> 2) & 3` |

The second prints words `+0x0c/+0x10/+0x14/+0x18` as `soft define` values. The
last `printk` result remains in `W0` but is not a semantic return value.

## Caller Context

Seven direct callers are TX/debug paths in `cpu_net_nb_tx`, `net_gso_upload_send`,
`net_tcp_gso_tx`, `idm_omci_tx`, `idm_cpu_tx`, and `idm_check_all_tx_desc`.

## Evidence

- Complete ARM64 body at `0x148b4` through `0x14938`.
- Literal format strings at `0x23e52` and `0x23e9b`.
- Direct xrefs at `0xd10c`, `0xe744`, `0xf6c0`, `0xf7c8`, `0x149ec`, `0x14b48`,
  and `0x14d64`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- Descriptor field names come from the vendor format string; bit-level hardware
  meanings beyond the displayed extraction remain unproven.
