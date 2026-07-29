# 0x11728 timer0_register_func

## Status

- Status: complete
- Confidence: verified callback store and identity return.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `timer0_cb_t timer0_register_func(timer0_cb_t callback)`.

## Semantics

Stores the input callback in `timer0_func` and returns the same pointer. No
locking, null rejection, allocation, or lifetime management is present. The
timer tasklet invokes this callback with no arguments when non-null.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
