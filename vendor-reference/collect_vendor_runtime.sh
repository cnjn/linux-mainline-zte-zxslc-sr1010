#!/bin/sh
# Collect read-only runtime evidence from the factory SR1010 system.

set -u
umask 077
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/kmodule/bin
export PATH

timestamp=$(date -u '+%Y%m%dT%H%M%SZ' 2>/dev/null || date '+%Y%m%d-%H%M%S')
output=${1:-/tmp/sr1010-vendor-runtime-${timestamp}}
case "$output" in
    /*) ;;
    *) output="$(pwd)/$output" ;;
esac

if [ -e "$output" ]; then
    echo "error: output already exists: $output" >&2
    exit 2
fi

mkdir -p "$output"
errors="$output/errors.log"
: >"$errors"

note_error() {
    echo "$*" >>"$errors"
}

copy_file() {
    source_path=$1
    relative_path=$2
    destination="$output/$relative_path"
    if [ ! -r "$source_path" ]; then
        note_error "unreadable: $source_path"
        return 0
    fi
    mkdir -p "$(dirname "$destination")"
    if ! cat "$source_path" >"$destination" 2>>"$errors"; then
        note_error "copy failed: $source_path"
    fi
}

capture() {
    relative_path=$1
    shift
    destination="$output/$relative_path"
    mkdir -p "$(dirname "$destination")"
    if ! "$@" >"$destination" 2>&1; then
        note_error "command failed: $*"
    fi
}

safe_name() {
    echo "$1" | sed 's#^/##; s#/#__#g; s#[^A-Za-z0-9_.@:+-]#_#g'
}

{
    echo "collector=sr1010-vendor-runtime-v1"
    echo "timestamp_utc=$timestamp"
    echo "uid=$(id -u 2>/dev/null || echo unknown)"
    echo "output=$output"
    echo "policy=read-only kernel interfaces; no MTD data, devmem, register dumps, or configuration changes"
} >"$output/metadata.txt"

capture system/date.txt date
capture system/uname.txt uname -a
capture system/uptime.txt uptime
capture system/hostname.txt hostname
capture system/mount.txt mount
capture system/df.txt df -k
capture system/busybox.txt busybox

for path in \
    /etc/os-release \
    /etc/version \
    /etc/ver_num_des \
    /etc/autokernelconf \
    /proc/version \
    /proc/cmdline \
    /proc/cpuinfo \
    /proc/meminfo \
    /proc/iomem \
    /proc/ioports \
    /proc/interrupts \
    /proc/devices \
    /proc/misc \
    /proc/partitions \
    /proc/mtd \
    /proc/modules \
    /proc/kallsyms \
    /proc/mounts \
    /proc/filesystems \
    /proc/sys/kernel/tainted \
    /proc/sys/kernel/randomize_va_space \
    /proc/sys/kernel/kptr_restrict
do
    copy_file "$path" "system${path}"
done

copy_file /proc/config.gz kernel/config.gz
copy_file /sys/kernel/notes kernel/notes
copy_file /sys/kernel/btf/vmlinux kernel/vmlinux.btf
capture kernel/dmesg.txt dmesg

# This is the post-U-Boot-fixup FDT that the running vendor kernel received.
copy_file /sys/firmware/fdt device-tree/runtime.dtb
if [ -d /proc/device-tree ]; then
    if ! tar -cf "$output/device-tree/proc-device-tree.tar" -C /proc device-tree 2>>"$errors"; then
        note_error "failed to archive /proc/device-tree"
    fi
else
    note_error "missing: /proc/device-tree"
fi
capture device-tree/proc-device-tree-files.txt find /proc/device-tree -type f -print

# Kernel resource ownership and driver binding.
capture buses/platform-devices.txt ls -la /sys/bus/platform/devices
capture buses/platform-drivers.txt ls -la /sys/bus/platform/drivers
capture buses/amba-devices.txt ls -la /sys/bus/amba/devices
capture buses/amba-drivers.txt ls -la /sys/bus/amba/drivers
capture buses/mdio-devices.txt ls -la /sys/bus/mdio_bus/devices
capture buses/mdio-drivers.txt ls -la /sys/bus/mdio_bus/drivers
capture buses/pci-devices.txt ls -la /sys/bus/pci/devices
capture buses/usb-devices.txt ls -la /sys/bus/usb/devices

for device in /sys/bus/platform/devices/* /sys/bus/amba/devices/*; do
    [ -d "$device" ] || continue
    name=$(safe_name "$device")
    directory="buses/devices/$name"
    mkdir -p "$output/$directory"
    for attribute in modalias uevent resource numa_node dma_mask coherent_dma_mask; do
        copy_file "$device/$attribute" "$directory/$attribute"
    done
    {
        echo "device=$device"
        echo "driver=$(readlink "$device/driver" 2>/dev/null || true)"
        echo "of_node=$(readlink "$device/of_node" 2>/dev/null || true)"
        echo "subsystem=$(readlink "$device/subsystem" 2>/dev/null || true)"
    } >"$output/$directory/links.txt"
done

# IRQ controller assignment is often more precise than /proc/interrupts alone.
for irq in /sys/kernel/irq/[0-9]*; do
    [ -d "$irq" ] || continue
    irq_name=$(basename "$irq")
    for attribute in actions chip_name hwirq name node per_cpu_count type wakeup; do
        copy_file "$irq/$attribute" "irq/$irq_name/$attribute"
    done
done

# MTD geometry and ECC counters only. This deliberately never opens /dev/mtd*.
for mtd in /sys/class/mtd/mtd[0-9]*; do
    [ -d "$mtd" ] || continue
    mtd_name=$(basename "$mtd")
    for attribute in \
        name type flags size erasesize writesize subpagesize oobsize numeraseregions \
        ecc_strength ecc_step_size corrected_bits failed bad_blocks bbt_blocks
    do
        copy_file "$mtd/$attribute" "mtd/$mtd_name/$attribute"
    done
done

# debugfs is not mounted by this script. If present, only documented summaries
# are read; raw regmap/register files are intentionally excluded.
if grep -q ' /sys/kernel/debug debugfs ' /proc/mounts 2>/dev/null; then
    copy_file /sys/kernel/debug/clk/clk_summary debugfs/clk_summary.txt
    copy_file /sys/kernel/debug/gpio debugfs/gpio.txt
    copy_file /sys/kernel/debug/regulator/regulator_summary debugfs/regulator_summary.txt
    copy_file /sys/kernel/debug/pm_genpd/pm_genpd_summary debugfs/pm_genpd_summary.txt
    copy_file /sys/kernel/debug/wakeup_sources debugfs/wakeup_sources.txt
    copy_file /sys/kernel/debug/suspend_stats debugfs/suspend_stats.txt
    copy_file /sys/kernel/debug/usb/devices debugfs/usb-devices.txt
    for controller in /sys/kernel/debug/pinctrl/*; do
        [ -d "$controller" ] || continue
        controller_name=$(safe_name "$controller")
        for attribute in \
            pins pingroups pinmux-functions pinmux-pins gpio-ranges \
            pinconf-groups pinconf-pins pinctrl-handles pinctrl-maps
        do
            copy_file "$controller/$attribute" "debugfs/pinctrl/$controller_name/$attribute"
        done
    done
else
    note_error "debugfs directory is missing; clock/pinctrl/GPIO summaries were not collected"
fi

# Thermal, CPU-frequency, and reset/clock-visible state.
for zone in /sys/class/thermal/thermal_zone* /sys/class/thermal/cooling_device*; do
    [ -d "$zone" ] || continue
    zone_name=$(basename "$zone")
    for attribute in type temp policy mode available_policies cur_state max_state stats/time_in_state; do
        copy_file "$zone/$attribute" "power-thermal/$zone_name/$attribute"
    done
done
for cpu in /sys/devices/system/cpu/cpu[0-9]*; do
    [ -d "$cpu" ] || continue
    cpu_name=$(basename "$cpu")
    for attribute in \
        scaling_available_frequencies scaling_available_governors scaling_cur_freq \
        scaling_driver scaling_governor cpuinfo_cur_freq cpuinfo_max_freq cpuinfo_min_freq
    do
        copy_file "$cpu/cpufreq/$attribute" "power-thermal/$cpu_name/cpufreq/$attribute"
    done
done

# Network topology and driver/PHY identity. All ethtool operations are queries.
copy_file /proc/net/dev network/proc-net-dev.txt
copy_file /proc/net/wireless network/proc-net-wireless.txt
copy_file /proc/net/vlan/config network/proc-net-vlan-config.txt
if command -v ip >/dev/null 2>&1; then
    capture network/ip-link.txt ip -details -statistics link show
    capture network/ip-address.txt ip address show
    capture network/ip-route.txt ip route show table all
    capture network/ip-neighbour.txt ip neighbour show
fi
if command -v bridge >/dev/null 2>&1; then
    capture network/bridge-link.txt bridge -details link show
    capture network/bridge-vlan.txt bridge -details vlan show
    capture network/bridge-fdb.txt bridge fdb show
fi
for interface in /sys/class/net/*; do
    [ -d "$interface" ] || continue
    interface_name=$(basename "$interface")
    directory="network/interfaces/$interface_name"
    mkdir -p "$output/$directory"
    for attribute in address addr_assign_type carrier carrier_changes dormant duplex flags \
        ifindex iflink mtu operstate speed type uevent
    do
        copy_file "$interface/$attribute" "$directory/$attribute"
    done
    {
        echo "device=$(readlink "$interface/device" 2>/dev/null || true)"
        echo "driver=$(readlink "$interface/device/driver" 2>/dev/null || true)"
        echo "of_node=$(readlink "$interface/device/of_node" 2>/dev/null || true)"
        echo "phydev=$(readlink "$interface/phydev" 2>/dev/null || true)"
    } >"$output/$directory/links.txt"
    if command -v ethtool >/dev/null 2>&1; then
        capture "$directory/ethtool.txt" ethtool "$interface_name"
        capture "$directory/ethtool-driver.txt" ethtool -i "$interface_name"
        capture "$directory/ethtool-features.txt" ethtool -k "$interface_name"
    fi
done

# Preserve loaded-module metadata and all vendor module binaries for analysis.
if [ -r /proc/modules ]; then
    cut -d ' ' -f 1 /proc/modules | while IFS= read -r module; do
        [ -n "$module" ] || continue
        directory="modules/loaded/$module"
        mkdir -p "$output/$directory"
        for attribute in coresize initsize refcnt srcversion taint version; do
            copy_file "/sys/module/$module/$attribute" "$directory/$attribute"
        done
        for parameter in /sys/module/"$module"/parameters/*; do
            [ -f "$parameter" ] || continue
            copy_file "$parameter" "$directory/parameters/$(basename "$parameter")"
        done
        if command -v modinfo >/dev/null 2>&1; then
            capture "$directory/modinfo.txt" modinfo "$module"
        fi
    done
fi

module_list="$output/modules/module-files.txt"
mkdir -p "$output/modules"
: >"$module_list"
for root in /lib/modules /usr/lib/modules /kmodule; do
    [ -d "$root" ] || continue
    find "$root" -type f \( -name '*.ko' -o -name '*.ko.gz' -o -name '*.ko.xz' \) -print \
        >>"$module_list" 2>>"$errors"
done
sort -u "$module_list" >"$module_list.sorted"
mv "$module_list.sorted" "$module_list"
while IFS= read -r module_path; do
    [ -r "$module_path" ] || continue
    destination="$output/modules/files/${module_path#/}"
    mkdir -p "$(dirname "$destination")"
    if ! cat "$module_path" >"$destination" 2>>"$errors"; then
        note_error "module copy failed: $module_path"
    fi
done <"$module_list"
capture modules/firmware-files.txt find /lib/firmware -type f -print

if command -v sha256sum >/dev/null 2>&1; then
    (
        cd "$output" || exit 1
        find . -type f ! -name SHA256SUMS -print | sort | while IFS= read -r path; do
            sha256sum "$path"
        done
    ) >"$output/SHA256SUMS"
else
    note_error "sha256sum is unavailable"
fi

archive="$output.tar.gz"
if tar -czf "$archive" -C "$(dirname "$output")" "$(basename "$output")" 2>>"$errors"; then
    echo "collection directory: $output"
    echo "archive: $archive"
else
    note_error "tar.gz creation failed; use the collection directory directly"
    echo "collection directory: $output"
fi

if [ "$(id -u 2>/dev/null || echo 1)" != 0 ]; then
    echo "warning: collection was not run as root; inspect errors.log for missing evidence" >&2
fi
