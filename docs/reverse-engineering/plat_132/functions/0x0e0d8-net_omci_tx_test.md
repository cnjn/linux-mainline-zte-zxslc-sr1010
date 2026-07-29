# 0x0e0d8 net_omci_tx_test

## Status

- Status: complete
- Confidence: verified allocation, byte pattern, logging, send, and free flow;
  semantic void ABI is a strong inference from incoherent machine return state.
- Size: `0xb0` bytes, 42 ARM64 instructions.
- Recovered signature: `void net_omci_tx_test(unsigned int length)`.

## Role

Global diagnostic sender for the shared OAM/OMCI management TX path. It creates
a byte ramp, prints every byte, sends the copied payload through port 2, then
releases the temporary source allocation.

## Semantics

The function calls `__kmalloc(length + 10, 0xa20)`. A null result only prints
`"alloc data error\n"` and exits. On success it prints
`"send omci/oam data:"`, fills bytes `0` through `length - 1` with their
truncated index values, prints each as `%02x `, then prints a newline.

It calls `net_tst_tx(data, length, 2)` and ignores its status. Because the
helper copies the payload into an skb before returning, freeing this temporary
source buffer immediately afterward is safe. There is no input length bound,
overflow check for `length + 10`, rate limit, or send-status reporting.

The allocation-failure path retains `printk`'s residual register value and the
success path retains a value from `kfree`; neither forms a coherent public
return contract, so the recovered semantic ABI is `void`.

## Callers and Concurrency

No direct IDA callers are present. The runtime kallsyms entry is global. The
function has no local synchronization; `net_tst_tx` and `cpu_net_tx` own the
TX allocation and queue synchronization.

## Evidence

- Complete ARM64 body at `0x0e0d8` through `0x0e184`.
- Allocation flags and `length + 10` at `0x0e0dc`-`0x0e0f8`.
- Byte fill/print loop at `0x0e130`-`0x0e14c`.
- Fixed port-2 send at `0x0e15c`-`0x0e168`, followed by `kfree`.
- Vendor strings and runtime kallsyms symbol.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The intended external diagnostic interface and any practical caller-imposed
  maximum payload length remain unknown.
