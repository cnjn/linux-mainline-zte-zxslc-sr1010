# ZX279133 IDM Descriptor Contract

## Scope

This document defines only the descriptor fields needed for one CPU-facing WAN
RX queue and the vendor CPU TX queue. Controlled single-packet TX is now
authorized by the transition below; RX-depth encoding remains a separate
blocker.

## Ring Geometry

- IDM exposes 24 RX queues and four TX queues.
- Descriptor mode zero uses a fixed 32-byte stride for RX and TX.
- Vendor Linux assigns WAN/SW transmission to hardware TX queue 1 (`cpu_tq`).
- Vendor CPU RX polls normal queues 0 through 7 and their jumbo counterparts 8
  through 15. Queue 0 is the first minimal normal-RX candidate.
- RX and TX descriptor addresses are 32-bit. Mainline must retain the existing
  32-bit DMA mask and use DMA API addresses rather than `virt_to_phys()`.

U-Boot TFTP hardware state independently confirms mode zero and 32-byte entries.
Its TX region starts at `0x9f7f8000`, its RX region at `0x9f818000`, and the
`0x20000` difference is exactly four TX queues times 1024 descriptors times 32
bytes. U-Boot programs IDM `0x00c = 0x04000000` and `0x070 = 0x000003e8`.
The TX depth therefore has direct value 1024. The RX value is not yet portable:
vendor Linux reconstruction writes `configured_depth - 1`, while U-Boot's
observed ring uses the literal 1000. Mainline must not infer an arbitrary small
RX depth until this encoding is tested.

## RX Descriptor

| Offset | Required meaning | Evidence |
| --- | --- | --- |
| `0x00` | 32-bit packet-buffer DMA address; nonzero means a completed descriptor | `cpu_net_rx`; U-Boot RX entries |
| `0x04[13:0]` | received frame length | `cpu_net_rx`; captured `0x05ea` = 1514 bytes |
| `0x05[6]` | normal/jumbo refill-pool selector | refill and jumbo accounting paths |
| `0x05[7]` | management/OMCI-OAM classification | verified vendor branch; deferred for WAN |
| `0x06[5:0]` | source-port code | RX histogram and skb source-port propagation |
| `0x08..0x17` | parsed packet metadata | vendor RX consumers; U-Boot captured IPv4 source/destination words |

The U-Boot queue-0 snapshot after a 4,417,724-byte TFTP transfer begins with:

```text
9f818000: 9f8d8000 078605ea c0a80164 c0a80101
9f818010: 00000000 00000c00 00000000 00000000
```

The first word is a distinct buffer address. The low 14 bits of the second word
are `0x5ea` (1514), and words two and three are `192.168.1.100` and
`192.168.1.1`, matching the TFTP peer and board.

RX ownership has no observed standalone owner bit. Hardware publishes a nonzero
word zero. Software consumes the descriptor, arranges refill ownership, clears
word zero, executes `DSB ST`, then writes `count | (queue << 12)` to IDM `0x088`.
It publishes normal/jumbo refill counts at `0x100`.

## TX Descriptor

| Offset | Required meaning | Evidence |
| --- | --- | --- |
| `0x00` | 32-bit packet-data DMA address | `idm_cpu_tx`; U-Boot TX entries |
| `0x04[14:1]` | frame length; bit zero and bit 15 are preserved controls | `idm_cpu_tx` |
| `0x07[5:0]` | destination encoding selector | `idm_cpu_tx` |
| `0x08` | base control word `0x00400000` for CPU TX | `idm_cpu_tx`; U-Boot snapshot |
| `0x0a[5:0]` | destination port value | `idm_cpu_tx` |
| `0x10`, `0x14` | zero for direct non-GSO PON TX | `cpu_net_pon_set_desc` |
| `0x18` | direct PON control word `0x08000000` | `cpu_net_pon_set_desc` |
| `0x1a[8:0]` | optional PON QoS queue; zero without callback | `cpu_net_pon_set_desc` |

The U-Boot TX snapshot contains repeated 32-byte entries:

```text
9f7f8000: 9fb18000 0f80005c 00440000 00000000
9f7f8010: 00000000 00000000 00000000 00000000
```

`0x005c >> 1` is 46 bytes, consistent with a small TFTP acknowledgement. Vendor
Linux uses hardware TX queue 1 for the physical WAN/SW path. After descriptor
writes it executes `DSB ST` and writes `0x20000` to the queue doorbell.

Software owns a TX slot until submission. Completion is not encoded in the
descriptor: IDM exposes a wrapping 16-bit completed count (`0x084` for queue 0,
then queue-specific registers). Software computes the modulo-65536 delta,
advances its owner-table consumer, frees each DMA/skb owner, and decrements the
pending count.

## Endianness and Coherency

- Descriptor words are consumed as native little-endian 16/32-bit values on
  ARM64. Both reconstructed code and U-Boot snapshots agree.
- RX free-buffer ring entries are different: vendor code applies `swab32()` to
  each 32-bit payload address before publishing it. They must be represented as
  big-endian ring entries, not reused as descriptor words.
- Vendor Linux writes `0x00200020` to sysctrl `0x78` and `0x7c`, then uses direct
  mappings with no descriptor or payload cache maintenance. U-Boot leaves
  `0x00000020` and can use explicit bootloader cache handling. This is strong
  evidence that bit 21 participates in Linux IDM coherency, but its exact field
  contract is not named.
- Mainline must use `dma_alloc_coherent()` for descriptors and refill rings plus
  `dma_map_single()`/`dma_unmap_single()` for packet data. It must not add
  `dma-coherent` or enable DMA until sysctrl bit-21 behavior is validated.

The dedicated `zte,zx279133-idm-cci` syscon lifecycle was tested on FIT SHA256
`2f6a36eabd99d1b4229a51f1dd80ea1998fc5e350f827225bf991fd272c83945`.
Both words changed from the U-Boot handoff `0x00000020` to `0x00200020` before
the MAC/PCS/SerDes path started, then returned exactly to `0x00000020` after
netdev stop. This validates register ownership and ordering. A later controlled
TX transition advanced the queue-1 completion count for three coherent
descriptors and three streaming-mapped payloads, validating DMA visibility for
the TX direction.

## Minimal Mainline Shapes

The implementation may use a raw eight-word descriptor while names remain
limited to proven fields:

```c
struct zx279133_idm_desc {
	__le32 address;
	__le32 length_flags;
	__le32 metadata[6];
};
```

Compile-time size and alignment checks must require exactly 32 bytes. RX/TX
software queue state must keep producer, consumer, pending, depth, DMA base, and
an owner array outside the hardware descriptor.

## Remaining Blockers

1. Determine whether the validated dedicated syscon sequence is an acceptable
   final binding or needs a narrower SoC integration interface.
2. Resolve the RX depth-minus-one discrepancy between vendor Linux
   reconstruction and U-Boot's literal `0x3e8` handoff.
3. Capture the WAN source-port value in RX byte `0x06` and confirm that TX
   destination outport 6 reaches XMAC1 once NPPT forwarding is initialized.
4. Recover the minimum NPPT/FFE/TM forwarding setup before treating an empty RX
   queue as an IDM failure.

## Per-Queue Doorbell and Completion Register Map

Recovered from `idm_cpu_nb_tx_update()` and `idm_get_tx_done()`
(`plat_132.ko`). All offsets are byte offsets from `nppt_base + 0x280000`.

| Queue | TX doorbell | Value | TX completion counter |
| --- | --- | --- | --- |
| 0 | `0x080` | `count << 17` | `0x084` |
| 1 | `0x0a0` | `count << 17` | `0x0ac` |
| 2 | `0x0a4` | `count << 17` | `0x0b0` |
| 3 | `0x0a8` | `count << 17` | `0x0b4` |

Queue n > 0 uses `4 * (n + 39)` for the doorbell and `4 * (n + 42)` for the
completion counter. Completion counters are truncated to 16 bits by the
vendor. Vendor Linux CPU TX (`idm_cpu_tx`) rings queue 1's doorbell; the
OMCI/OAM path uses queue 0 and the Wi-Fi path queue 2.

## Vendor Linux IDM Register Image

Values below are the exact CPU133 `idm_init()` program, verified from
`plat_132.ko` disassembly with constants fixed by `kernel-2b5.elf` data.
Offsets are byte offsets from `nppt_base + 0x280000`.

Control word `0x000` RMW sequence (executed early):

```text
|= 0x000f0000
= (v & 0xf00fffff) | 0x00f00000
|= 0x00003000
```

Fixed writes:

| Offset | Value | Note |
| --- | --- | --- |
| `0x010` | `128` | |
| `0x014..0x034` | `0x00800080` | nine words |
| `0x038` | `50000` | |
| `0x054` | `0x06060606` | |
| `0x058` | `0x00060606` | |
| `0x05c` | `0x07070707` | |
| `0x060` | `0x07070707` | |
| `0x074` | `0x00000210` | |
| `0x090` | `20` | |
| `0x094` | `1` | |
| `0x124` | `0` | |
| `0x3fc` | `0x00000f49` | |
| `0x5c0` | `7` | CPU133/129 only |

Descriptor-region configuration (executed last):

```text
0x0c0 = uNPPT_IDM_DESC_MODE      (BSS, no writer found => 0)
0x000 = (v & 0x8fffffff) | (uIDM_RX_CFG_DEPTH << 28)
0x070 = uIDM_RX_QUEUE_DESC_DEPTH - 1
0x008 = reserved_base
0x00c = uIDM_TX_CFG_DEPTH << 16   (BSS, no writer found => 0)
0x004 = reserved_base + 24 * 32 * uIDM_RX_QUEUE_DESC_DEPTH
```

`kernel-2b5.elf` fixes the constants: `uIDM_RX_QUEUE_DESC_DEPTH = 2048`,
`uIDM_RX_CFG_DEPTH = 1`, `uIDM_TX_QUEUE_DESC_DEPTH = 1024`,
`uNPPT_IDM_TX_QUEUE_NUM = 4`, `uNPPT_IDM_RX_QUEUE_NUM = 24`. The two BSS
globals (`uNPPT_IDM_DESC_MODE`, `uIDM_TX_CFG_DEPTH`) have no writer in the
kernel or any module, so both are zero at runtime. The final control word
image from a U-Boot `0x00ff0007` handoff is therefore `0x10ff3007`. U-Boot's
active-TFTP word is `0x01ff0007` (bit 24 set); vendor Linux never touches bit
24.

Note: earlier mainline revisions wrote `50000`/`20`/`7` to `0x218`/`0x250`/
`0x2c0` (statistics-register addresses, register indices `0x86`/`0x94`/`0xb0`
times four) and omitted `0x3fc`, `0x0c0`, and `0x00c`. Those were not vendor
behavior and are corrected.
