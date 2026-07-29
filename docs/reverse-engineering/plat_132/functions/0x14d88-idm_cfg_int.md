# 0x14d88 idm_cfg_int

## Status

- Status: complete
- Confidence: verified for register writes, IRQ request sequence, error paths,
  and target-CPU state; strong inference for affinity bitmap layout.
- Size: `0x26c` bytes, 153 ARM64 instructions.
- Recovered signature: `int idm_cfg_int(void)`.

## Role

This function completes the IDM interrupt backend setup for `idm_init`. It
programs four IDM words, requests four hard IRQs in sequence, and records IRQ
CPU placement for two- or four-CPU systems.

## Initial Register Writes

Relative to `nppt_base + 0x280000`, it writes the current `idm_info` words in
this non-monotonic source-field order:

| Register offset | Source data |
| --- | --- |
| `0x044` | `idm_info + 0x0` |
| `0x048` | `idm_info + 0x4` |
| `0x04c` | `idm_info + 0xc` |
| `0x050` | `idm_info + 0x8` |

No hardware field names are established for these locations.

## IRQ Registration

Every request has a null thread handler, flags 0, and null `dev_id`.

| `g_idm_irq` index | Handler | Name |
| --- | --- | --- |
| 0 | `idm_cpu_int` | `"cpu"` |
| 1 | `idm_wifi_int` | `"idm"` |
| 2 | `idm_rls_int` | `"buf_rls"` |
| 3 | `idm_all_int` | `"localtest"` |

The requests execute in table order. Any negative result logs the matching
error text and returns that exact negative value. There is no cleanup of a
previously successful IRQ request, no masking rollback, and no affinity cleanup
after a later failure. Nonnegative request results are treated as successful.

The vendor runtime probe log confirms the four input IRQs as 26 through 29.

## CPU Affinity

After all four requests succeed, affinity-hint return values are ignored.
Adjacent 32-bit globals at `0x264f8` through `0x26504` record the intended
CPU number for each IRQ:

| `nr_cpu_ids` | IRQ 0 | IRQ 1 | IRQ 2 | IRQ 3 |
| --- | --- | --- | --- | --- |
| 4 | 1 | 2 | 2 | 3 |
| 2 | 0 | 1 | 0 | 1 |

The raw pointer operands for `irq_set_affinity_hint` lie at successive 64-bit
slots following the imported `cpu_bit_bitmap` symbol. Cross-reference to
`lower_net_smb_test_config @ 0x10778` uses the same address pattern as
`cpu_bit_bitmap[cpu + 1]`, which is the basis for the source-like mapping. IDA
also labels some of those relocated slots as IDM configuration imports, so their
original source-level names and exact relocation semantics remain unverified.

For any CPU count other than 2 or 4, the function optionally rate-limits and
logs `"idm err: NR_CPUS 2\n"` using literal `2`, then still returns zero.

## Error Behavior and Ownership

- Register writes happen before any IRQ request and are not undone on failure.
- A failure at IRQ N leaves all successful requests 0 through N-1 installed.
- A successful affinity setup leaves all IRQs installed and returns zero even
  when `irq_set_affinity_hint` reports an error.
- `idm_init` treats only a negative return from this function as failure.

## Evidence

- Full ARM64 disassembly at `0x14d88` through `0x14ff0`.
- Direct caller reconstruction in `idm_init @ 0x14ff4`.
- Xrefs to `g_idm_irq`, each handler, target-CPU globals, and `cpu_bit_bitmap`.
- `lower_net_smb_test_config @ 0x10778` and
  `set_idm_int_cpu_rx_cpu_config @ 0x139a4` for independent affinity-pointer
  and target-CPU evidence.
- Vendor runtime log records IDM IRQ values 26 through 29.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The exact hardware meaning of the four initial IDM register words is unknown.
- `cpu_bit_bitmap` import relocation and the overlapping IDA symbol labels need
  vendor link-map or kernel image evidence before their original declarations
  can be named confidently.
- Teardown behavior for these IRQs has not yet been reconstructed.
