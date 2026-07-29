# 0x0fa68 testftp_net_report

## Status

- Status: complete
- Confidence: verified metadata flags, packet arithmetic, counters, logging,
  external-call arguments, return paths, and both direct callers; protocol
  labels are limited to byte-layout evidence.
- Size: `0x104` bytes, 65 ARM64 instructions.
- Recovered signature:
  `int testftp_net_report(const void *data, const u8 *metadata, u32 received_length)`.

## Role

Classify a CPU RX testftp candidate by two metadata bits, derive a transport
payload length from packet headers, optionally log it, and forward the result to
external `ffe_pre_process_zte`.

## Inputs and Return Contract

The function first increments `testftp_cnt`. It reads `metadata[6]` and follows
these paths:

- When bit 2 is set, it treats `data + metadata[5]` as an IPv4-shaped header:
  header length is `4 * (header[0] & 0x0f)`, total length is big-endian
  `header[2:3]`, and the forwarded payload length is total length minus the
  IP-header length and `4 * (transport[12] >> 4)`.
- Otherwise, when bit 1 is set, it treats `data + metadata[5]` as an
  IPv6-shaped header: transport begins 40 bytes later, payload length is
  big-endian bytes `+4:+5`, and the forwarded length subtracts the transport
  header length encoded in `transport[12]`.
- When neither bit is set, it increments and returns
  `testftp_unhandled_type_count` without logging or calling the external hook.

All arithmetic uses unchecked unsigned subtraction. The task argument is
`metadata[7]`; valid paths call:

```c
ffe_pre_process_zte(task, payload_length, received_length, transport_header)
```

and propagate its 32-bit return unchanged.

## Logging and Concurrency

When signed `testftp_debug_cnt > 0`, the function decrements it and prints task,
derived payload length, header length field, and received length. The counters
and debug budget use ordinary load/add/store sequences with no lock or atomic
operation.

## Caller Context

- `cpu_net_rx @ 0x0c5dc` calls this for an ordinary RX frame when its local
  testftp discriminator is `0xff`; it then returns the backing IDM buffer and
  clears the descriptor regardless of this return value.
- `sub_FA60 @ 0x0fa60` is a physical PAN-toggle fall-through wrapper.

## Evidence

- Complete ARM64 body at `0x0fa68` through `0x0fb68`.
- Metadata-byte tests at `0x0fa8c`, `0x0fa90`, and `0x0fad0`.
- Header-length, big-endian length, and transport-data-offset arithmetic on both
  branches.
- Counter/debug accesses and exact `ffe_pre_process_zte` argument registers at
  `0x0fb48` through `0x0fb58`.
- CPU RX caller and vendor kallsyms symbols for the counters and function.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact descriptor metadata bit names and the policy implemented by the external
  `ffe_pre_process_zte` provider.
- Why CPU RX discards the helper's return after buffer release.
