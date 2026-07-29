# 0x096a8 uni_serdes_set_sprbsrxbist

## Status

- Status: complete
- Confidence: verified first-call snapshot, asymmetric restore target, exact
  enable predicate, child calls, persistent counter return, and exported ABI.
- Size: `0xdc` bytes, 53 ARM64 instructions.
- Recovered signature:
  `int uni_serdes_set_sprbsrxbist(int prbs_mode, int rx_bist_enable)`.

## Semantics

On the first call, snapshots Uni SerDes offsets `0x24`, `0x48`, and `0x94`.
The persistent call counter is incremented and returned on every invocation.

When `rx_bist_enable == 1`, it calls:

1. `uni_serdes_set_rx_prbs_mode(prbs_mode - 1)`
2. `uni_serdes_set_check_en(1)`
3. `uni_serdes_set_err_cnt_en(1)`

For every other enable value, it restores the saved `+0x48` and `+0x94` words.
Notably, the word originally saved from `+0x24` is restored to `+0x14`, an
asymmetry retained exactly from the binary.

## Caller Context

No internal IDB xrefs target this exported entry. It is exported through
`__ksymtab_uni_serdes_set_sprbsrxbist`.

## Evidence

- Complete ARM64 body at `0x96a8` through `0x9780`.
- First snapshot at `0x96c8`-`0x96e8`: source offset `0x24`, plus `0x48` and
  `0x94`.
- Exact enable-one gate at `0x96f8`-`0x96fc`; child calls at `0x9704`, `0x970c`,
  and `0x9714`.
- Restore writes at `0x9748`, `0x9750`, and `0x9754`; first target is `+0x14`.
- Persistent counter increment at `0x976c`-`0x9774`.
- IDA type at `0x96a8` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
