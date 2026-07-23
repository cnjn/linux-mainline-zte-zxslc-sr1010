# SR1010 vendor reference

This directory contains reproducible static and runtime evidence from the
official SR1010 V1.0.0.2B5 firmware.

## Static extraction

`extract_vendor_reference.py` validates the OTA container before writing any
artifact. It checks both header CRCs, encrypted kernel/rootfs CRCs, AES block
alignment and decryption, the FIT and board FDT structures, any FIT image hashes
that are present, the ARM64 Image header, JFFS2 magic, and the reconstructed ELF
layout. The official 2B5 FIT has no per-image `hash@...` nodes; this is recorded
as an empty hash map in the manifest instead of being reported as verified.

The extractor requires Python 3 with `pycryptodome`, `dtc`, and
`vmlinux-to-elf`. This checkout already has `vmlinux-to-elf` 1.3.6 under
`raw/.venv`; otherwise put it on `PATH` or pass `--vmlinux-to-elf` explicitly.

Run from the repository root:

```sh
python3 port/vendor-reference/extract_vendor_reference.py \
  --input firmwares/zxhnsr1010_hv100_fv1002b58000_firmware-20260618145154.bin \
  --output-dir port/vendor-reference/2b5 \
  --force
```

The important outputs are:

- `Image`: the gzip-decoded ARM64 kernel image.
- `zx279133-sr1010.dtb`: the board DTB embedded in the selected FIT config.
- `zx279133-sr1010.dts`: the same DTB decompiled with `dtc`.
- `kernel-2b5.elf`: a symbolized ELF reconstructed from embedded kallsyms.
- `vendor-2b5.itb`: the decrypted FIT, trimmed to its FDT total size.
- `manifest.json`: source offsets, addresses, validation metadata, and hashes.
- `SHA256SUMS`: hashes for every binary and generated reference artifact.

The FIT load and entry address are physical `0x80080000`. The reconstructed ELF
uses the kernel's linked virtual address `0xffffffc010080000`; its entry is the
recovered `start_kernel` symbol at `0xffffffc010ab0994`. Do not rebase the ELF to
the FIT physical address in IDA or Ghidra.

Verify the emitted files from the extraction directory:

```sh
shasum -a 256 -c SHA256SUMS
file Image zx279133-sr1010.dtb kernel-2b5.elf vendor-2b5.itb rootfs.jffs2
```

Recompiling the vendor DTS with `dtc` emits warnings for malformed PCIe `reg`
lengths, non-standard unit names, and clock/reset cells that are not valid
phandle references. These warnings are properties of the official DTB. The
decompile/recompile round trip is byte-identical, so keep this file unchanged as
evidence and fix the corresponding nodes only in the separate mainline DTS.

## Factory runtime collection

The FIT DTB is only the static input. U-Boot can fix it up before Linux receives
it, and driver state is only authoritative on a running factory system. Copy
`collect_vendor_runtime.sh` to the device and run it as root:

```sh
chmod 700 /tmp/collect_vendor_runtime.sh
sh /tmp/collect_vendor_runtime.sh /tmp/sr1010-vendor-runtime
```

Retrieve `/tmp/sr1010-vendor-runtime.tar.gz` and place the unpacked directory
under `port/vendor-reference/sr1010-vendor-runtime/`. The collection includes:

- `/sys/firmware/fdt` and an archive of `/proc/device-tree`.
- `/proc/iomem`, interrupts, platform/AMBA binding, IRQ chips, and kallsyms.
- MTD geometry, ECC strength/step size, corrected/failed counts, and bad-block
  counts, without opening any `/dev/mtd*` data device.
- Clock, regulator, power-domain, pinctrl, GPIO, thermal, and wakeup summaries
  when debugfs is already mounted.
- Network interfaces, driver/PHY links, MDIO devices, and query-only `ethtool`
  results.
- Loaded-module metadata plus copies of vendor `.ko` files from `/lib/modules`,
  `/usr/lib/modules`, and `/kmodule`.

## Accepted 2B5 runtime baseline

`sr1010-vendor-runtime/` was collected as root from the factory 2B5 system and
has been accepted as the live reference for mainline porting. Of the 924 entries
in its `SHA256SUMS`, 923 verify. The sole mismatch is `errors.log`: the device ran
out of space while creating the final `tar.gz`, then appended that failure to the
log after its hash had already been recorded. The copied collection directory
and all engineering evidence are intact; no repeat collection is required.

The live `/sys/firmware/fdt` is a valid 40,960-byte DTB. A sorted structural
comparison against the 37,374-byte FIT DTB found only these U-Boot fixups:

- `/chosen/bootargs` changes from the static bring-up value to
  `console=ttyAMA0,115200n8 root=/dev/mtdblock8 ro rootfstype=jffs2 mem=512M serial=open pcie_pme=nomsi`.
- `/chosen/versioninfo` is populated with the current slot and V1.0.0.2B5.8000
  firmware metadata.
- `/soc/spifc@0x10d0f000/status` changes from `disabled` to `okay`.

No other logical DT property or node differs. The live kallsyms addresses also
independently confirm the reconstructed ELF mapping:

```text
ffffffc010080000 T _text
ffffffc010ab0000 T __init_begin
ffffffc010ab0994 T start_kernel
ffffffc010b30000 D __init_end
```

The most important runtime hardware evidence is:

- Both watchdog instances are bound to `zx_wdt` and started by the factory
  driver with a 16-second heartbeat and a reported 1024 Hz watchdog clock.
  `clk_summary` shows both WDT0 and WDT1 clock paths enabled.
- The SPI-NAND is a 128 MiB Winbond device with 128 KiB erase blocks, 2 KiB
  pages, 64-byte OOB, and ECC strength 4 bits per 512-byte step.
- All MTD devices report zero bad blocks and zero BBT-reserved blocks. The
  cumulative corrected-bit counters are 8 for `rootfs2`, 6 for `Plugin`, and 0
  for the other partitions. This vendor kernel does not expose the `failed`
  counter, so absence of uncorrectable errors must not be inferred from it.
- UART0 is bound through AMBA to `uart-pl011`; both MDIO controllers, both
  watchdogs, all five GPIO banks, pinctrl, and the SPI flash controller have
  confirmed live driver bindings.
- The collection contains the important vendor modules, including
  `plat_132.ko`, `np.ko`, `rtl8373_switch.ko`, `rlt8226b.ko`, `zx279051.ko`,
  `switch.ko`, and `bspdriver.ko`.

Known non-blocking gaps are preserved rather than treated as successful reads:

- `/proc/device-tree` is a symlink on this system, so
  `proc-device-tree.tar` contains only that symlink. The complete authoritative
  `device-tree/runtime.dtb` is present, so this does not require a rerun.
- The factory kernel provides neither `/proc/config.gz` nor BTF data.
- No usable `ethtool` command was present, and two extended BusyBox `ip`
  invocations were unsupported. Interface sysfs state, addresses, routes, and
  driver topology were still collected; link-specific data can be gathered
  during the later Ethernet bring-up phase.
- The device clock was not initialized, so collected file times and metadata
  start in 1970. This does not affect content integrity.

The collector does not mount debugfs itself. If the first archive reports that
debugfs is not mounted, a second, fuller collection can be made with:

```sh
mount -t debugfs debugfs /sys/kernel/debug
sh /tmp/collect_vendor_runtime.sh /tmp/sr1010-vendor-runtime-debugfs
umount /sys/kernel/debug
```

Only run the final `umount` when the preceding `mount` was done specifically for
this collection. The script never invokes `devmem`, reads raw MTD contents,
dumps register files, writes sysfs/debugfs controls, or changes network state.
The archive can contain device identifiers such as MAC addresses and serial
numbers from the live FDT, so treat it as private engineering data.
