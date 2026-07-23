#!/bin/sh
# ZXDBG diagnostic FIT builder for the SR1010 WAN bring-up.
#
# Self-contained: builds the mainline kernel + DTB with an embedded busybox
# initramfs (diagnostic init) and assembles a FIT loaded at 0x80200000.
# Runs the compile inside the stage15 container; re-execs itself in Docker
# when invoked from the host.
#
# Usage (from host):
#   /Volumes/code/zx279133/linux-6.18.38/port/mainline/build-zxdbg.sh
#
# Output: /Volumes/code/zx279133/out/sr1010-zxdbg.itb
#
# This whole file is part of the removable ZXDBG debug scaffolding.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ZXROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)       # .../zx279133
KERNEL_SRC=${KERNEL_SRC:-"$ZXROOT/linux-6.18.38"}        # .../zx279133/linux-6.18.38
IMAGE=${IMAGE:-sr1010-stage15-builder}
OUT=${OUT:-"$ZXROOT/out"}
BUSYBOX=${BUSYBOX:-"$ZXROOT/busybox"}
IPERF3=${IPERF3:-"$ZXROOT/iperf3"}
JOBS=${JOBS:-4}
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}

# ---- Re-exec inside the builder container ---------------------------------
if [ "${ZXDBG_INNER:-0}" != 1 ]; then
	command -v docker >/dev/null 2>&1 || {
		echo "docker not found on host" >&2; exit 1; }
	docker image inspect "$IMAGE" >/dev/null 2>&1 || {
		echo "missing docker image: $IMAGE" >&2; exit 1; }
	exec docker run --rm \
		-e ZXDBG_INNER=1 \
		-e JOBS="$JOBS" \
		-e OUT="$OUT" \
		-e BUSYBOX="$BUSYBOX" \
		-e CROSS_COMPILE="$CROSS_COMPILE" \
                -e KERNEL_SRC \
		--ulimit nofile=8192:8192 \
		-v /Volumes/code:/Volumes/code \
		-w "$KERNEL_SRC" \
		"$IMAGE" \
		"$SCRIPT_DIR/build-zxdbg.sh"
fi

# ---- Inner build (inside container) ---------------------------------------
export CROSS_COMPILE ARCH=arm64
KERNEL_OUT="$OUT/kernel"
ROOTFS="$OUT/rootfs"

for tool in "${CROSS_COMPILE}gcc" make mkimage gzip dtc sha256sum; do
	command -v "$tool" >/dev/null 2>&1 || {
		echo "missing build tool: $tool" >&2; exit 1; }
done
[ -x "$BUSYBOX" ] || { echo "missing prebuilt busybox: $BUSYBOX" >&2; exit 1; }

mkdir -p "$KERNEL_OUT" "$OUT"

# ---- Assemble the initramfs root ------------------------------------------
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS/bin" "$ROOTFS/sbin" "$ROOTFS/usr/bin" "$ROOTFS/usr/sbin" \
	"$ROOTFS/proc" "$ROOTFS/sys" "$ROOTFS/dev"
cp "$BUSYBOX" "$ROOTFS/bin/busybox"
chmod 0755 "$ROOTFS/bin/busybox"
cp "$IPERF3" "$ROOTFS/bin/iperf3"
chmod 0755 "$ROOTFS/bin/iperf3"
install -m 0755 "$SCRIPT_DIR/initramfs/init" "$ROOTFS/init"

# gen_init_cpio spec: device nodes are declared here (initramfs has no /dev yet).
cat > "$OUT/initramfs.list" <<EOF
dir /dev 0755 0 0
nod /dev/console 0600 0 0 c 5 1
nod /dev/null 0666 0 0 c 1 3
dir /proc 0555 0 0
dir /sys 0555 0 0
dir /run 0755 0 0
dir /root 0755 0 0
dir /bin 0755 0 0
dir /sbin 0755 0 0
dir /usr 0755 0 0
dir /usr/bin 0755 0 0
dir /usr/sbin 0755 0 0
dir /tmp 0755 0 0
file /init $ROOTFS/init 0755 0 0
file /bin/busybox $ROOTFS/bin/busybox 0755 0 0
file /bin/iperf3 $ROOTFS/bin/iperf3 0755 0 0
EOF

# The init shebang is #!/bin/sh, so /bin/sh MUST resolve.  A bare busybox
# binary is not enough: create the applet symlinks the script relies on.
# (gen_init_cpio 'slink' entries; busybox --install can't run at build time.)
for applet in sh ash mount cat ls grep sleep ip ifconfig ping route \
	dmesg devmem cttyhack watchdog hostname printf kill uname reboot; do
	echo "slink /bin/$applet /bin/busybox 0755 0 0" >> "$OUT/initramfs.list"
done
for applet in init; do
	:
done

# ---- Configure the kernel --------------------------------------------------
KCONFIG_ALLCONFIG="$SCRIPT_DIR/zxdbg.fragment" \
	make -C "$KERNEL_SRC" O="$KERNEL_OUT" ARCH=arm64 allnoconfig

make -C "$KERNEL_SRC" O="$KERNEL_OUT" ARCH=arm64 scripts

"$KERNEL_SRC/scripts/config" --file "$KERNEL_OUT/.config" \
	--set-str INITRAMFS_SOURCE "$OUT/initramfs.list"

make -C "$KERNEL_SRC" O="$KERNEL_OUT" ARCH=arm64 olddefconfig

for want in CONFIG_ZTE_ZX279133_ETH=y CONFIG_ZX279051_PHY=y \
	CONFIG_MDIO_ZX279133=y CONFIG_PCS_XPCS=y CONFIG_PHYLINK=y \
	CONFIG_BLK_DEV_INITRD=y CONFIG_INET=y CONFIG_PACKET=y; do
	grep -qx "$want" "$KERNEL_OUT/.config" || {
		echo "required kernel setting missing: $want" >&2; exit 1; }
done

# ---- Build -----------------------------------------------------------------
make -C "$KERNEL_SRC" O="$KERNEL_OUT" -j"$JOBS" \
	Image zte/zx279133-sr1010.dtb

gzip -n -9 -c "$KERNEL_OUT/arch/arm64/boot/Image" > "$OUT/Image.gz"
cp "$KERNEL_OUT/arch/arm64/boot/dts/zte/zx279133-sr1010.dtb" \
	"$OUT/zx279133-sr1010.dtb"

# ---- Assemble the FIT ------------------------------------------------------
sed \
	-e "s|@KERNEL_IMAGE@|$OUT/Image.gz|g" \
	-e "s|@FDT@|$OUT/zx279133-sr1010.dtb|g" \
	"$SCRIPT_DIR/sr1010-mainline.its.in" > "$OUT/sr1010-zxdbg.its"
mkimage -f "$OUT/sr1010-zxdbg.its" "$OUT/sr1010-zxdbg.itb" >/dev/null

FIT_SIZE=$(wc -c < "$OUT/sr1010-zxdbg.itb")
FIT_END=$((0x80200000 + FIT_SIZE))
sha256sum "$OUT/sr1010-zxdbg.itb" > "$OUT/sr1010-zxdbg.itb.sha256"

echo
echo "=== ZXDBG FIT built ==="
echo "FIT:        $OUT/sr1010-zxdbg.itb"
echo "size:       $FIT_SIZE bytes"
echo "load/entry: 0x80200000"
echo "sha256:     $(cut -d' ' -f1 "$OUT/sr1010-zxdbg.itb.sha256")"
