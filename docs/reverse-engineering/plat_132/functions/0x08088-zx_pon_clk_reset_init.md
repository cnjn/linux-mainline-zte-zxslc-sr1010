# 0x08088 zx_pon_clk_reset_init

## Status

- Status: complete
- Confidence: verified exported API ABI, mode propagation, CPU reset ordering,
  all delay counts, downstream status handling, caller context, and void ABI.
- Size: `0x214` bytes, 131 ARM64 instructions.
- Recovered signature: `void zx_pon_clk_reset_init(uint32_t mode)`.

## Semantics

Passes `mode` to `pon_pll_cfg`, stores it in `pon_serdes_mode`, then performs a
CPU-specific CRM reset pulse before invoking `pon_serdes_init(mode)`.

- CPU 129 and CPU 133 clear `top_crm_base + 0x70` bits 0 then 1, delay ten
  times with `__const_udelay(0x418958)`, set bit 0 then bit 1, and delay ten
  more times.
- CPU 132 performs the same `+0x70` sequence, also clears `+0x60` bit 9 before
  the first delay and sets it between the two `+0x70` release writes.
- Other CPU types skip the reset pulse but still call `pon_serdes_init(mode)`.

It logs whether `pon_serdes_init` failed but always returns the raw zero
residual. Existing module declarations and all known callers treat this exported
entry as semantic `void`.

## Caller Context

The main probe calls it for selected PON modes at `0xc0c` through `0xcf0`; the
recovered `uni_serdes_init @ 0xae34` also calls it at `0xaf1c`. The symbol is
exported as `zx_pon_clk_reset_init` in vendor kallsyms.

## Evidence

- Complete ARM64 body at `0x8088` through `0x8298`.
- `pon_pll_cfg(mode)` call at `0x809c` and `pon_serdes_mode` store at `0x80a4`.
- CPU-129 pulse at `0x80cc`-`0x812c`, CPU-132 pulse at `0x8170`-`0x81e8`, and
  CPU-133 pulse at `0x8214`-`0x8274`.
- Each phase initializes a counter to 11 and uses pre-decrement looping, giving
  ten `0x418958` delay calls per phase.
- `pon_serdes_init` call and success/failure log selection at `0x8138`-`0x8284`.
- IDA type at `0x8088` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
