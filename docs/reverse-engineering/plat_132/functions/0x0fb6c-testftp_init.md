# 0x0fb6c testftp_init

## Status

- Status: complete
- Confidence: verified store, caller, and semantic void ABI.
- Size: `0x10` bytes, 4 ARM64 instructions.
- Recovered signature: `void testftp_init(void)`.

## Semantics

Stores literal one to `g_speedtesthffenable` and returns. The machine retains
the global address in `x0`, but its only direct caller ignores that residual;
the semantic ABI is void.

## Caller Context

`cpu_net_init @ 0x0e220` calls this after netdev/NAPI registration and before
GRO and GSO initialization. No synchronization, allocation, callback, MMIO, or
ownership transfer occurs in the function itself.

## Evidence

- Complete four-instruction body at `0x0fb6c` through `0x0fb78`.
- Literal-one store to imported/global `g_speedtesthffenable`.
- Sole direct `cpu_net_init` caller.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.
