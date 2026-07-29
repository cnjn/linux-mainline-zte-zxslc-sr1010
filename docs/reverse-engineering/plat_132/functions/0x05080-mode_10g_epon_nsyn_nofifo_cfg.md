# 0x05080 mode_10g_epon_nsyn_nofifo_cfg

## Status

- Status: complete
- Confidence: verified both raw profiles, all ordered 32-bit stores, CPU gates,
  common tail, ignored dispatcher arguments, caller context, and semantic void
  behavior.
- Size: `0x3bc` bytes, 198 ARM64 instructions.
- Recovered signature: `void mode_10g_epon_nsyn_nofifo_cfg(void)`.

## Semantics

Logs `mode_10g_epon_nsyn_nofifo_cfg`, then applies the non-synchronous 10G
EPON no-FIFO SerDes profile. CPU type 132 has priority over 133:

| CPU predicate | Profile writes | Shared tail |
| --- | --- | --- |
| `isCpuType_132() == 1` | 46 ordered 32-bit stores at `0x00..0xb4` | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | 46 different ordered 32-bit stores at `0x00..0xb4` | same three stores |
| neither | no MMIO writes | none |

The generic arguments from `serdes_mode_set` are unused. The script consists
exclusively of raw 32-bit stores, with no register reads or RMWs.

## Return Semantics

Supported paths leave a base-pointer residual in `w0`; unsupported paths leave
the CPU-133 predicate result. The logging result is discarded. These values do
not form an API result, so the recovered semantic ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` calls this entry only for switch case 3,
`MODE_10G_EPON_NSYN_NO_FIFO`.

## Evidence

- Complete ARM64 body at `0x5080` through `0x5438`.
- CPU-132 gate at `0x5094`-`0x509c`; CPU-133 fallback at `0x5244`.
- CPU-132 stores `0x50b8`-`0x523c`; CPU-133 stores `0x5268`-`0x5420`.
- Common tail writes at `0x5424`, `0x542c`, and `0x5430` are distinct 32-bit
  stores, not a wide write.
- The only direct caller is `serdes_mode_set` case 3 at `0x7d00`.
- IDA type at `0x5080` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
