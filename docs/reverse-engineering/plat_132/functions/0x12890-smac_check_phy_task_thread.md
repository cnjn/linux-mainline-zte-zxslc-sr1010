# 0x12890 smac_check_phy_task_thread

## Status

- Status: complete
- Confidence: verified stop predicate, ordered polling calls, sleep interval,
  return propagation, callback registration path, and lack of local state.
- Size: `0x5c` bytes, 23 ARM64 instructions.
- Recovered signature: `int smac_check_phy_task_thread(void *argument)`.

## Semantics

The thread argument is not read. In each iteration it calls
`kthread_should_stop()` and tests exactly `result & 0xff`. A nonzero low byte
ends the thread and returns the unmodified raw result. Other bits alone do not
terminate this loop according to the observed `TST W0, #0xff` instruction.

When continuing, the function invokes `check_phy` in this fixed, unrolled
order:

```c
check_phy(0);
check_phy(1);
check_phy(2);
check_phy(3);
check_phy(4);
check_phy(5);
check_phy(6);
```

It then calls `msleep_interruptible(100)` and repeats. The sleep return value is
discarded on the next stop check.

## Caller Context

`smac_thread_init @ 0x128ec` passes this function as the callback to
`kthread_create_on_node`, with a null argument. It is referenced as callback
data at `0x128f4` and `0x12900`; there is no direct in-module `BL` caller.

## Concurrency and Ownership

This is the periodic PHY-management context. It has no local lock, allocation,
MMIO access, or direct mutable global access. Synchronization and PHY state
ownership are delegated to `check_phy`; the sleep creates a nominal 100 ms
polling cadence, subject to scheduler interruption/delay.

## Evidence

- Complete 23-instruction ARM64 body at `0x12890` through `0x128e8`.
- `TST W0, #0xff` and branch to `RET` preserve the precise low-byte stop test.
- Seven explicit `BL check_phy` instructions with literals zero through six,
  followed by `MOV W0, #100; BL msleep_interruptible`.
- Callback references from the verified thread-creation helper.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether `kthread_should_stop` can return a nonzero value with a zero low byte
  in this vendor kernel ABI.
- Stop/join ownership and teardown sequencing outside this worker.
