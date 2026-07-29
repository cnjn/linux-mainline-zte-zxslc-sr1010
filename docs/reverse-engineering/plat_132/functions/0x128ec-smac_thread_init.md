# 0x128ec smac_thread_init

## Status

- Status: complete
- Confidence: verified worker/function arguments, error-pointer predicate,
  success and failure paths, fixed caller, diagnostic strings, and raw return
  register behavior; semantic void signature is a strong inference.
- Size: `0x50` bytes, 18 ARM64 instructions.
- Recovered signature: `void smac_thread_init(void)`.

## Semantics

Calls the imported thread-creation API with these exact live arguments:

```c
kthread_create_on_node(smac_check_phy_task_thread, NULL, -1,
                       "smac_check_phy_task");
```

It treats a returned pointer strictly above unsigned `-4096` as an error value.
The error path only prints `create smac_check_phy_task_thread failed!` and
returns. On success it forwards the returned task pointer unchanged to
`wake_up_process`, then prints `create smac_check_phy_task_thread ok!`.

No task pointer is stored in local module state by this function, no cleanup is
attempted, and the raw return register from either final `printk` call is not
normalized. Its sole caller discards that register and `nppt_smac_init` itself
returns zero, so the source-like void signature is a strong inference rather
than a verified exported ABI.

## Caller Context

`nppt_smac_init @ 0x129c8` is the sole direct in-module caller. It invokes this
helper after all SMAC and XMAC setup, regardless of CPU type or XMAC setup
results. The created worker is `smac_check_phy_task_thread @ 0x12890`.

## Concurrency and Ownership

This is the transition from single-threaded SMAC initialization to the periodic
PHY polling context. There is no local lock or task-reference retention. The
worker begins only after `wake_up_process`; failed creation leaves the callback
state initialized but unpolled.

## Evidence

- Complete 18-instruction ARM64 body at `0x128ec` through `0x12938`.
- Exact argument registers: worker in X0, null X1, `0xffffffff` in W2, and the
  task-name string in X3.
- `CMN X0, #0x1000` plus `B.HI` proves the raw error-pointer range test.
- Success preserves X0 from creation into `wake_up_process`; both paths converge
  at the final `printk` call.
- Sole caller xref and runtime dmesg success message.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether another module or later teardown code retains/stops this worker.
- Original source-level task type and exact error-pointer macro name.
