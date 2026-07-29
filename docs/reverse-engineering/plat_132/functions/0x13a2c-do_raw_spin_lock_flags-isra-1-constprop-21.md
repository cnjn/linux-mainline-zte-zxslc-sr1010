# 0x13a2c do_raw_spin_lock_flags.isra.1.constprop.21

## Status

- Status: complete
- Confidence: verified fixed-lock address, prefetch, exclusive acquire/retry,
  contended slow path, no-argument ABI, and both direct callers.
- Size: `0x4c` bytes, 19 ARM64 instructions.
- Recovered signature:
  `void do_raw_spin_lock_flags_idm_lock_int(void)`.

## Semantics

Acquires the fixed `idm_lock_int` queued spinlock. It issues a write prefetch,
uses `LDAXR` to observe the word, and retries `STXR` stores of literal one while
the observed word is zero. A nonzero observed word is passed unchanged to
`queued_spin_lock_slowpath(&idm_lock_int, observed, 0, 1)`. The entry takes no
meaningful argument even though its generated name resembles a flags-taking
helper.

On the uncontended path `X0` retains an unrelated address used to form the lock
pointer; no semantic result is returned.

## Caller Context

Only `idm_int_enable @ 0x13a78` and `idm_int_disable @ 0x13ad8` call this
entry, after DAIF save and before mutating the shared interrupt mask.

## Evidence

- Complete ARM64 body at `0x13a2c` through `0x13a74`.
- `PRFM`, `LDAXR`, `STXR`, retry, and slow-path arguments in the disassembly.
- Direct calls at `0x13a90` and `0x13af0`.

## Source-Like Reconstruction

`recovered/plat_idm.c` uses the valid semantic source identifier
`do_raw_spin_lock_flags_idm_lock_int`.
