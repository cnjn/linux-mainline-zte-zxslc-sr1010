# 0x14ff4 idm_init

## Status

- Status: complete
- Confidence: verified for control flow, allocation/error behavior, raw MMIO
  offsets, and call order; strong inference for queue/FIFO field labels.
- Size: `0xe70` bytes, 924 ARM64 instructions.
- Recovered signature: `int idm_init(void)`.

## Role and Call Context

`nppt_init` invokes this function as its final stage and bitwise-ORs the return
with earlier initialization statuses. `idm_init` constructs IDM reserved-memory
state, free-buffer FIFOs, RX/TX descriptor rings, IDM hardware registers, IDM
IRQs, and the CPU-facing network interface.

It returns zero on its success path and `-1` on locally recognized failures.
The successful path calls `cpu_net_init` but ignores that callee's return value,
so a CPU-netdev initialization failure can still make `idm_init` report success.

## CPU and Reserved Memory Setup

At entry the function selects `uIdm_Int_Rls`:

| CPU predicate | Value |
| --- | --- |
| `isCpuType_133()` or `isCpuType_129()` | `0x07000000` |
| `isCpuType_132()` | `0x03000000` |

It obtains the physical reserved region through external vendor-kernel exports
`get_idm_reserved_size` and `get_idm_reserved_base`. A zero base logs
`"alloc idm reserved mem failed\n"` and returns `-1`.

The required-size check is directly reconstructed as:

```c
required = 0x800 + free_ring_bytes + descriptor_bytes + 0x800 +
           normal_slots * (uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE + 320) +
           jumbo_slots * (uJUMBO_BP_SIZE - uSKB_SHAREDINFO_SIZE + 320);

descriptor_bytes = (uIDM_TX_QUEUE_DESC_DEPTH +
                    6 * uIDM_RX_QUEUE_DESC_DEPTH) << 7;
free_ring_bytes = 4 * (uIDM_RX_NORMAL_BP_NUM + uIDM_RX_JUMBO_BP_NUM +
                       uIDM_TX_JUMBO_BP_RETRV_NUM +
                       uIDM_TX_NORMAL_BP_RETRV_NUM +
                       uIDM_TX_EXTRAL_BP_RETRV_NUM);
```

The exact role of one 0x800-byte margin is not established by the later address
calculations. Insufficient capacity returns `-1` without cleanup.

After accepting the region, the function clears two 32-bit fields at offsets
`0x200` and `0x204` in `idm_free_data` for every possible CPU, then zeros
`idm_lock_int`, `idm_refill_lock`, and `idm_lock_cfg`.

## Queue and Allocation Contract

The function requires exactly 24 configured RX queues and four configured TX
queues. Any mismatch enters the rate-limited `idm_buffer_init` failure path and
returns `-1`.

It then:

1. Allocates four `buf_tq[i]` pointer tables, each containing
   `uIDM_TX_QUEUE_DESC_DEPTH` 64-bit entries. Every one is checked.
2. Allocates normal and jumbo software FIFO pointer arrays sized by
   `uIDM_TX_NORMAL_BP_NUM + uIDM_RX_NORMAL_BUFFER_NUM` and
   `uIDM_RX_JUMBO_BUFFER_NUM + uIDM_TX_JUMBO_BP_NUM`, respectively.
3. Checks only the normal FIFO allocation. A null jumbo FIFO allocation is not
   checked before later use.
4. Derives power-of-two FIFO masks through CLZ-based rounding, records buffer
   data lengths, and fills both FIFOs with linear-mapped buffer addresses via
   `idm_fifo_in`.
5. Creates `"idm buf cache"` and `"idm jbuf cache"` with the observed rounded
   object-size formulas and flag value `270336`. Neither cache-creation result
   is checked.

`idm_fifo_in` can report a full FIFO, but `idm_init` ignores all of those return
values. No failure branch frees any tables, FIFO arrays, cache objects, or
already initialized hardware state.

## IDM Hardware Programming

All offsets below are relative to `nppt_base + 0x280000`. Field names are not
recovered; offsets and values are verified from ARM64 instructions.

- The initial control word receives the bit operations `|= 0x000f0000`,
  `&= 0xf00fffff; |= 0x00f00000`, then `|= 0x00003000`.
- Fixed values are written at `0x054=0x06060606`, `0x058=0x00060606`,
  `0x05c=0x07070707`, `0x060=0x07070707`, `0x090=20`, `0x094=1`,
  `0x3fc=0x0f49`, `0x074=0x210`, `0x038=50000`, and `0x010=128`.
- Every 32-bit word from `0x014` through `0x034` receives `0x00800080`.
- CPU 133/129 additionally writes `7` at `0x5c0`.
- Buffer-pool configuration is emitted at `0x10c`, `0x110`, and, on CPU
  133/129, `0x40c`; each encodes configured pool counts divided by
  `uIDM_BP_CFG_UNIT`.
- Physical free-ring addresses are emitted at `0x104`, `0x108`, `0x118`, and
  `0x11c`, with the CPU-133/129 extra address at `0x408`.
- The normal/jumbo refill doorbell is `0x100`. Counts are sent after `DSB ST`
  in chunks no larger than 2048; jumbo counts occupy the upper 16 bits.
- Descriptor configuration uses offsets `0x0c0`, `0x000`, `0x070`, `0x008`, and
  `0x00c`; the second descriptor base written at `0x004` is
  `reserved_base + 24 * 32 * uIDM_RX_QUEUE_DESC_DEPTH`.
- The final IDM interrupt mask is written at `0x040` as
  `uIdm_Int_Rls | 0x00ffffff`. Offset `0x06c` receives
  `uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE - uBP_BUFFER_OFFSET - 63`.

## Ring Initialization

The function translates reserved physical addresses with:

```c
((u32)physical - memstart_addr) | 0xffffff8000000000ULL
```

It uses that mapping to initialize normal and jumbo RX buffer rings, invokes
`idm_rx_refill` once per configured RX buffer, and fails if any refill returns
a negative value. `idm_rx_refill` allocates a buffer, converts the payload
address to physical form, byte-swaps it, and stores it in the ring entry.

The inferred queue layouts are:

| Object | Observed layout/action |
| --- | --- |
| RX queue | 24 consecutive 16-byte entries: descriptor base, zeroed 32-bit field, descriptor depth. |
| TX queue | Four consecutive 40-byte entries: descriptor base, `buf_tq[i]`, three zeroed words, queue index, descriptor depth, final zero word. |

The descriptor bytes are explicitly zeroed before these entries are assigned.
The field labels in the recovered C are analyst labels only.

## Interrupt and Netdev Handoff

After ring construction, the function stores four NPPT IDM register pointers,
initializes the `idm_info` prefix to `65023`, `0x00ff0000`, and `512`, attaches
the `idm_ops` table, and calls `idm_cfg_int`.

A negative `idm_cfg_int` status returns `-1`; nonnegative statuses continue.
The final handoff is:

```c
cpu_register_netinfo(&idm_info);
pp_free_skb_data = idm_free_skb_data;
cpu_net_init();             /* Return value ignored. */
return 0;
```

## Runtime Corroboration

The vendor runtime boot log confirms the successful CPU-133 path:

- `alloc idm reserved mem size a00000/813800`
- `idm_reserved_base is 9d700000`
- `idm buf cache len 2432`
- `idm jbuf cache len 10176`
- `rx ring 800/1000 ..., jumbo rx ring 20/1000`
- `pp net init ok,share 320`

The same log reports IDM IRQs 26 through 29 during platform probe.

## Evidence

- Full Hex-Rays reconstruction and 924-instruction ARM64 disassembly of
  `idm_init @ 0x14ff4`.
- Targeted assembly validation of the reserved-size check, per-CPU reset,
  raw register programming, descriptor setup, and refill loops.
- Direct decompilation of `idm_cfg_int @ 0x14d88`, `idm_fifo_in @ 0x142b0`,
  `idm_rx_refill @ 0x1c460`, `cpu_register_netinfo @ 0xe1ec`, and
  `cpu_net_init @ 0xe220`.
- Vendor runtime log:
  `vendor-reference/sr1010-vendor-runtime/kernel/dmesg.txt`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The hardware meaning of every IDM register offset and packed pool-count
  field remains unknown.
- The exact vendor types behind `idm_info`, `idm_ops`, spinlocks, FIFO counters,
  and descriptor words require downstream data-plane analysis.
- The purpose of the extra 0x800-byte capacity margin, partial-init cleanup,
  and the intentionally unchecked jumbo FIFO/cache/netdev results need vendor
  source or companion-module evidence.
