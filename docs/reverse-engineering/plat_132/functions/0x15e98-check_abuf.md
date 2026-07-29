# 0x15e98 _check_abuf

## Status

- Status: complete
- Confidence: verified control flow, pool selection, address-to-index arithmetic,
  temporary bitmap handling, diagnostics, locking, and all direct callers.
- Size: `0x6dc` bytes, 418 ARM64 instructions.
- Recovered signature: `static void _check_abuf(uint8_t pool_selector)`.

## Semantics

The low bit of `pool_selector` selects normal (`0`) or jumbo (`1`) buffer
configuration. The checker derives a pool base, stride
`BP_SIZE + 0x140 - uSKB_SHAREDINFO_SIZE`, RX refill ring, RX buffer count, and
the total count `RX_BUFFER_NUM + TX_BP_NUM`.

It allocates a temporary `unsigned long` bitmap with allocation flags `0xdc0`,
then marks buffer indices observed in these locations:

1. The selected IDM FIFO, while holding its qspinlock under the observed
   `SP_EL0 + 0x10 += 0x200` bottom-half bracket.
2. Both producer (`+0x000/+0x200`) and consumer (`+0x100/+0x204`) entries of
   every possible CPU's `idm_free_data` stash.
3. `RX_BUFFER_NUM` entries walked backwards from the selected refill index in
   the selected byte-swapped RX refill ring, wrapping at `RX_BP_NUM`.

Each observed address becomes `(address - pool_base) / stride`; an unsigned
comparison against the total pool count rejects invalid addresses. A previously
set bitmap bit logs a repeated buffer. It prints aggregate FIFO/kmem status
counters and reports absent entries. Normal missing entries dump 128 payload
bytes at `uBP_BUFFER_OFFSET + 64`; jumbo missing entries only log
`Jumbo buf not support yet!` with the vendor kernel log-level prefix.

## Preserved Edge Behavior

- The allocated bitmap is never explicitly cleared before `test_and_set_bit`.
- Allocation reserves one additional 64-bit word, but the final report scans
  only `total_buffer_count >> 6` words, omitting any partial final group.
- The all-seen fast path compares a 64-bit bitmap word to `0x0fffffff`, then
  counts 64 entries.
- The detailed 64-bit scan uses a 32-bit variable shift followed by sign
  extension. Bits 32 through 63 repeat the low 32 shift positions, and bit 31
  includes sign-extended high bits. The recovered C models this explicitly.
- Per-CPU stash and refill-ring reads occur without a snapshot lock. The stash
  scan runs for jumbo mode too, even though the stashes are populated by normal
  pool high-context paths.
- There is no evidenced semantic return contract; the allocation-failure and
  final `kfree` residual register values are not modeled as a return value.

## Caller Context

- `idm_check_bppe @ 0x16574` forwards its byte argument and is exported in the
  collected runtime kallsyms.
- `check_bppe @ 0x16588` always calls this helper with zero, selecting normal
  buffers.

## Evidence

- Complete ARM64 body at `0x15e98` through `0x16570`.
- Direct caller xrefs at `0x1657c` and `0x16594`.
- All literal diagnostics at `0x241ed` through `0x242fb`, including kernel log
  prefixes `\0014` and `\001c`.
- Existing verified `idm_fifo`, `idm_free_data`, RX refill, `idm_status`,
  byte-swap, and atomic-bit helper reconstructions.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
