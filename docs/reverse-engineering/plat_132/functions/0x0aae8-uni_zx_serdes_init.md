# 0x0aae8 uni_zx_serdes_init

## Status

- Status: complete
- Confidence: verified profile dispatch, all RMW and polling order, CPU/mode
  lock conditions, three 1001-iteration timeout paths, final CPU-129 check,
  return values, and caller context.
- Size: `0x1c8` bytes, 101 ARM64 instructions.
- Recovered signature: `int uni_zx_serdes_init(uint32_t mode)`.

## Semantics

Initializes the Uni SerDes for a mode and returns zero on success or `-1` on a
lock timeout.

1. Calls `uni_serdes_mode_set(mode)`, logs its completion, then RMW-sets
   `uni_serdes_base + 0x54` bit 0.
2. Polls a PLL status with 1001 attempts and `__const_udelay(0x8312b0)`:
   CPU 133 modes 5/6 require `+0xcc[2:0] == 7`; other CPU 133 modes require
   `+0xd0[0]`; all non-133 CPUs require `+0xcc[1]`.
3. Logs ALOS from `+0xe4[0]`, then polls CDR lock at `+0xe4[1]` with the same
   delay/retry policy.
4. On CPU 129 only, waits until either `+0xe4[9]` or `+0xe4[10]` is set. Each
   loop test retains two separately volatile reads, matching the assembly.

Each failed poll logs its corresponding vendor message and returns `-1`; a
successful path returns zero. No register-field meaning beyond the observed
lock and vendor log context is assigned.

## Caller Context

`uni_serdes_init @ 0xae34` is the sole direct caller at `0xaed8`. The function
is local text (`t`) in runtime `kallsyms`.

## Evidence

- Complete ARM64 body at `0xaae8` through `0xacac`.
- Profile dispatch at `0xab00`, completion log at `0xab08`-`0xab10`, and
  `+0x54` bit-zero RMW at `0xab14`-`0xab20`.
- CPU-133 mode-5/6 status loop at `0xab30`-`0xab70`; CPU-133 ordinary status
  loop at `0xab80`-`0xabcc`; non-133 status loop at `0xaba0`-`0xabcc`.
- ALOS diagnostic at `0xabdc`-`0xac08`; CDR loop at `0xac0c`-`0xac30`.
- CPU-129 two-read status loop at `0xac58`-`0xac84`, testing bits 9 and 10
  through distinct `+0xe4` loads.
- Common `-1` return at `0xac84`-`0xac8c` and zero return at `0xac9c`.
- IDA type at `0xaae8` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
