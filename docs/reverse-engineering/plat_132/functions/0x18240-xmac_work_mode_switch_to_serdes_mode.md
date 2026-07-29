# 0x18240 xmac_work_mode_switch_to_serdes_mode

## Status

- Status: complete
- Confidence: verified all mode mappings, output-write behavior, invalid path,
  and lack of callers.
- Size: `0x84` bytes, 32 ARM64 instructions.
- Recovered signature: `int xmac_work_mode_switch_to_serdes_mode(
  int work_mode, int *serdes_mode)`.

## Semantics

Maps XMAC work modes to SerDes mode values:

| XMAC mode | SerDes mode |
| --- | --- |
| 0 | 0 |
| 1 | 2 |
| 2, 3 | 7 |
| 4, 7 | 4 |
| 5 | 1 |
| 6 | 3 |
| 8 | 5 |

Valid modes store through the unchecked output pointer and return zero. Every
other signed or unsigned value logs `"unspport xmac work mode %d"`, does not
write the output pointer, and returns `-1`.

## Caller Context

No direct module callers were found. Runtime kallsyms marks it as local text.

## Evidence

- Complete ARM64 body at `0x18240` through `0x182c0`.
- Nine-case jump table at `0x1e63c` and all output writes.
- Invalid-mode diagnostic at `0x24652`.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
