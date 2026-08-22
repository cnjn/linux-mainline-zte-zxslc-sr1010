# ZX279133 consolidated patch series

This directory contains a compact 16-patch series from the Linux 6.18.38 source
baseline to the validated ZX279133/SR1010 production snapshot.

The repository history contains 78 original development commits in this range.
They are deliberately consolidated into 16 functional stages rather than
exported as 78 one-commit patches.

## Base and target

Official Linux baseline:

- signed tag: `v6.18.38`
- tag commit: `e46dc0adfe39724bcf52cea47b8f9c9aed86a394`
- source tree: `d13b0d25dbbc19af5884d0b780c74309c5d3fa1e`

The project history was root-rewritten earlier. Its local baseline is:

- local baseline commit: `68acd82bb42ff6cec6468d96cd8833f6f0ba8ac3`
- source tree: `d13b0d25dbbc19af5884d0b780c74309c5d3fa1e`

The local root and signed tag have different commit IDs but byte-identical
source trees, so the series applies to either baseline.

Production target:

- canonical target commit: `88c15ca1f28369b54feb0987e36acb429d4b226a`
- target tree: `74dd64ba6762d092a7af1b24cf286aed918b19ae`

The two later research-only commits are intentionally excluded:

- `4db01bd770ac037def7a5dc83d49fcf2c7a35530` — RTL8372N MDIO port audit
- `db8df607c79cfbd077e3978916d7eeff72efee4c` — Linux 6.18 research notes

## Functional stages

1. initial UART, clock, watchdog, DTS and defconfig platform support;
2. thermal/PVT, GPIO status controls and initial USB enablement;
3. fail-closed read-only/partitions-only SPI NAND support;
4. xHCI CCI clock ownership;
5. corrected LSP1 clocks, eFuse/NVMEM calibration and second watchdog;
6. firmware-selected top-clock model;
7. PWM pinctrl, clocks, controller and bounded audit support;
8. ZX279133 MDIO clocks, controller and DTS nodes;
9. SR1010 ZX279051 and RTL8372N read-only MDIO audits;
10. network clocks plus NPPT/XPCS bindings and audit nodes;
11. PON/Uni SerDes, XPCS runtime-PM and ZX279051 PHY support;
12. NPPT, RTL8372N and network-syscon bindings;
13. decomposed ZX279133 NPPT Ethernet driver;
14. LAN conduit, private transport tagger and RTL8372N DSA driver;
15. final SR1010 networking DTS topology;
16. final arm64 defconfig enablement.

## Archive files

- `SERIES`: 16-patch application order;
- `COMMITS`: consolidated patch commit and filename map;
- `ORIGINAL-COMMITS`: maps all 78 original commits to their consolidated patch;
- `validation/`: preserved validation records from the earlier incremental
  networking series;
- `SHA256SUMS`: checksums for every archive file except the manifest itself.

## Apply and verify

```sh
git checkout --detach v6.18.38
while IFS= read -r patch; do
        git am "/path/to/upstream-series/$patch"
done < /path/to/upstream-series/SERIES
```

The resulting tree must be:

```text
74dd64ba6762d092a7af1b24cf286aed918b19ae
```

Verify the archive with:

```sh
sha256sum -c upstream-series/SHA256SUMS
```

## Validated networking artifact

The final networking state was board-tested on SR1010 using FIT SHA-256:

`57157ef89349344b0a2957f58dbabe8419f755ba1c37514b960860fc4dbec615`

The matching installed RTL8372N module had SHA-256:

`e918a29d56ac1cb7b342ce237feeb21fd5bfd949eec4e847a480fbb1cd86df67`
