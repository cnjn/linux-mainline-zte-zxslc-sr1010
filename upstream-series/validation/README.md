# Preserved incremental validation records

The `incremental/` directory retains the 15 validation records from the earlier
16-patch networking-only archive. Their internal patch numbers refer to that
old archive, whose first patch represented original development commit 63.

Mapping into the consolidated 16-patch series:

```text
old incremental 0001-0002 -> consolidated patch 0010
old incremental 0003-0008 -> consolidated patch 0011
old incremental 0009-0011 -> consolidated patch 0012
old incremental 0012      -> consolidated patch 0013
old incremental 0013-0014 -> consolidated patch 0014
old incremental 0015      -> consolidated patch 0015
old incremental 0016      -> consolidated patch 0016
```

The old archive had no separate `0001-validation.txt`; all 15 available records
are retained byte-for-byte. `ORIGINAL-COMMITS` is the authoritative mapping for
all 78 original commits.
