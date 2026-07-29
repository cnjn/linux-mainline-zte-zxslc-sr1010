# 0x18a30 xpcs_eee_cfg

Status: complete. For selectors zero through four, writes `0x81df` to offset
`0x0e0018`, `0x1cf2` to `0x0e0020`, and either `0x2ffa` (byte profile exactly
one) or `0x35fa` (all other profile values) to `0x0e0024`. Invalid selectors
log `xmac_index(%d) is error` and return. The full 65-instruction ARM64 body,
all direct/base-relative address paths, and no direct caller xrefs were verified.
IDA type is `void xpcs_eee_cfg(u8 xmac, u8 profile)`.
