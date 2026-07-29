# 0x03160 get_all_efuse

## Status

- Status: complete
- Confidence: verified CPU gate, all 33 raw register dumps, every decoded
  field, format-string difference between the two branches, per-argument read
  order, and the constant zero return.
- Size: `0xfac` bytes, 850 ARM64 instructions.
- Recovered signature: `int get_all_efuse(void)`.

## Semantics

Diagnostic dump of the whole efuse block. A single `isCpuType_129()` test
selects one of two mutually exclusive branches, so exactly one layout is
printed per call. Both branches follow the same overall shape:

1. `REG VAL`: 33 raw words at `efuse_base + 0x00 .. 0x80`, labeled with the
   physical addresses `0x14f11000 .. 0x14f11080`.
2. `ATE`: wafer-test identity fields decoded from words `0x00`-`0x18`.
3. `BOARD`: secure-boot protection bits from word `0x1c`, then key material.
4. `LEFT_PROTECT`: 15 reserved write-protect bits.
5. `ATE IP`: process/trim characterization fields.
6. `STATUS & CONFIG`: two bits from word `0x80`.

The branches are not just a formatting difference. They decode different
layouts, which is the durable finding here.

| Region | CPU 129 | Other CPUs |
| --- | --- | --- |
| Format specifier | `%#x` | `%#u` |
| Word `0x1c` bits 5-8 | HUK write/read protect, hash-key protect, reserved | backup-AES present/protect, reserved |
| `0x30`-`0x3c` | HUK (hardware unique key) | backup AES secret key |
| `0x40`-`0x5c` | HASH key, 8 words | `0x40` is the reserved-protect word |
| `0x60`-`0x64` | anti-rollback counter | not decoded |
| `0x68` | `chip_status`, low byte | not decoded |
| `0x6c` | reserved-protect word | not decoded |
| `0x70` | `T_TRIM`, `POR`, `IDDQ_CP`, `Chip_Type` | not decoded |
| `0x44`-`0x50` | part of HASH key | `T25_DATA`, `V_DATA`, `P_SVT/LVT/HVT`, `PON_mode`, `IDDQ_CP`, `BIN_CP`, `Package_Type`, `Bin`, `A53_bin`, `IDDQ_FT1/FT2`, `GEPHY_temo_coef`, `PPU_CLK` |

Shared decodings in both branches: `crc_ate_cp1` = `0x00[7:0]`, `x_addr` =
`0x00[15:8]`, `y_addr` = `0x00[23:16]`, `wafer_no` = `0x08[12:8]`,
`chip_version` = `0x18[11:8]`, `FT_write_pw` = `0x18[31]`, and word `0x1c`
bits 0-4 as safety-boot enable, CPU JTAG protect, safety-boot protect, and
AES-key write/read protect.

`lot_id` and `host_name` are printed with `0x%02x%x%02x` in both branches,
even in the `%#u` branch, and each is assembled from three separate words:
`lot_id` from `0x08[7:0]`, all of `0x04`, and `0x00[31:24]`; `host_name` from
`0x18[7:0]`, all of `0x14`, and `0x10[31:24]`.

## Multi-Word Field Reads

Two fields are stitched from two independent 32-bit loads rather than one wide
access:

- `crc_ate_ft1`: `UBFIZ` of `0x0c[4:0]` to bits 7-3 combined with `0x08[31:29]`
  via `ORR ... LSR#29`.
- `system_time`: `LDR W1,[X0,#0xC]`, `LDR W0,[X0,#0x10]`, then
  `EXTR W1, W0, W1, #0x18`, that is `(word_0x10 << 8) | (word_0x0c >> 24)`.

Hex-Rays renders `system_time` as `*(_QWORD *)(efuse_base + 12) >> 24`. That
is arithmetically equivalent for the low 32 bits but is not what the machine
code does, and a literal unaligned 64-bit load would be wrong on this target.
The reconstruction keeps the two-load form.

## MMIO Access Pattern

Every printed value re-reads its word; nothing is cached across `printk`
calls. The 129 branch performs 98 word reads from 78 base-pointer loads, and
the other branch 99 reads from 87 loads. The difference is the multi-word
fields, where one `LDR X0, [X19,#efuse_base]` feeds several offset loads
before a single call.

Repeat counts include one read for the raw `REG VAL` dump. The most re-read
words are the reserved-protect word at 16 reads, one dump plus 15 protect
bits, and word `0x1c` at 10 reads on 129 and 9 on the other branch. Word
`0x80` is read 3 times in both. On 129, `0x70` is read 5 times, one dump plus
`T_TRIM`, `POR`, `IDDQ_CP`, and `Chip_Type`.

The reconstruction preserves each read because efuse reads are observable MMIO
accesses, and it uses 197 `EFUSE_U32` expansions matching the 197 machine
reads.

## Return Semantics

Both branches converge on `MOV W0, #0` at `0x40ec`, so the function always
returns 0 regardless of CPU type or register contents.

## Security Note

This routine prints raw secret key material to the kernel log: the AES secret
key, and either the HUK plus HASH key (CPU 129) or the backup AES secret key
(other CPUs). Anything with access to dmesg or a persisted console log can
recover these values. This behavior is recorded as observed vendor behavior;
it is not reproduced as a recommendation.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. Consistent
with the other `check_*`/dump routines in this address range, it appears to be
a retained diagnostic entry point reachable only from an external debug hook.

## Evidence

- Complete ARM64 body at `0x3160` through `0x410a`, 850 instructions.
- Branch gate at `0x3190`-`0x31b8`: `BL isCpuType_129`, `CMP W0, #1`,
  `B.NE loc_38F4`.
- Call and read census, machine versus reconstruction. 129 branch
  `0x31bc`-`0x38f3`: 83 `BL printk`, 98 word reads. Other branch
  `0x38f4`-`0x40e3`: 92 calls, 99 reads. Shared tail at `loc_40E4`: 1 call.
  Machine total is 176 calls; the reconstruction has 177 because the shared
  tail call is written once in each C branch. Per-offset read multiplicities
  match exactly in both branches.
- Per-field bitfield instructions confirmed directly, including `UXTB`,
  `UBFX`, `UBFIZ`, `LSR`, `ORR ... LSR#29`, and `EXTR`.
- Eight-argument `HASH Key` call at `0x3674`-`0x36a4`, where the ninth value
  (`0x40`) is passed on the stack via `STR W0, [SP,#0x10+var_10]`.
- Shared tail at `loc_40E4` reached by `B` from `0x38f0`.
- IDA type at `0x3160` set to the recovered signature and Hex-Rays cache
  invalidated to confirm it.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
