#!/bin/sh
# Boot a kernel on the SR1010 with kexec, without flashing.
#
# Usage: kexec-boot.sh <Image on the build host>
#
# The Image must be built from the same kernel.config as the flashed firmware
# (release/candidates-kexec-20261013/kernel.config), or the =m modules in the
# root file system will not load. "reboot" on the router returns to the
# flashed kernel. See docs/mainline-network/KEXEC_TEST_LOOP.md.
set -eu

IMAGE=${1:?usage: kexec-boot.sh <Image on build host>}

BUILD="ssh -o BatchMode=yes -p 2022 192.168.223.19"
# While the router WAN is cabled to the laptop; with the laptop elsewhere, go in
# over the LAN side: ROUTER="ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -J 192.168.167.93 root@192.168.5.1"
ROUTER=${ROUTER:-"ssh -o BatchMode=yes -o ConnectTimeout=5 -J 192.168.167.100 -p 2022 root@192.168.174.5"}
# The mac's en1 does not go through the router, so it stays up while the
# router restarts; the board console is its tmux session.
MAC="ssh -o BatchMode=yes 192.168.167.93"
PANE=sr1010-serial:0
TAG="KEXEC-$(date +%s)"

echo "== copy $IMAGE"
want=$($BUILD "sha256sum '$IMAGE'" | cut -d' ' -f1)
$BUILD "cat '$IMAGE'" | $ROUTER 'cat > /tmp/Image-test'
got=$($ROUTER 'sha256sum /tmp/Image-test' | cut -d' ' -f1)
[ "$want" = "$got" ] || { echo "sha256 mismatch: $want != $got" >&2; exit 1; }

echo "== load"
# The SR1010 kernels are built with CONFIG_CMDLINE_FORCE, so a command line
# given to kexec is ignored: put test settings into the test kernel's own
# CONFIG_CMDLINE instead (see KEXEC_TEST_LOOP.md). A kexec'ed kernel without
# nosmp tried to start CPU1, which the vendor PSCI cannot bring back after a
# kexec, counts it as stuck and refuses any further kexec.
FIRST=$($ROUTER "if ! kexec -s -l /tmp/Image-test >&2; then \
		dmesg | tail -1 >&2; \
		echo 'cannot kexec again: reboot first, or give the test kernels nosmp in CONFIG_CMDLINE' >&2; \
		exit 1; fi; \
	[ \"\$(cat /sys/devices/system/cpu/online)\" = 0-1 ] && echo yes || echo no")

# Every kernel started from here, the flashed one included, must stop the
# NPPT in its shutdown hook (zx279133_eth_shutdown): nothing else stops the
# IDM DMA, which then writes into memory the next kernel already uses: from
# a firmware without the hook one kexec in two hung for good. A hook that
# unprepared the whole NPPT was worse, the board reset about a second after
# every kexec; the one that only closes the MACs works.
echo "== execute through the serial console ($TAG, from the flashed kernel: $FIRST)"
# Run it from the console shell: a command started over ssh is killed when
# the ssh session closes, before kexec -e runs. After a boot the console
# waits for Enter; what is typed before its shell prompt appears is lost.
$MAC "export PATH=/opt/homebrew/bin:\$PATH; \
	tmux send-keys -t $PANE '' Enter; \
	for i in \$(seq 1 20); do \
		tmux capture-pane -pJt $PANE | grep -v '^\s*\$' | tail -1 | grep -qE '^root@.*# *\$' && break; \
		sleep 0.5; done; \
	tmux send-keys -t $PANE -l 'echo $TAG; sync; kexec -e'; \
	tmux send-keys -t $PANE Enter; sleep 2; \
	tmux capture-pane -pJt $PANE -S -50 | grep -qx '$TAG'" || {
	echo "the console did not take the command" >&2; exit 1; }

echo "== wait for the console and for ssh"
$MAC "export PATH=/opt/homebrew/bin:\$PATH; \
	for i in \$(seq 1 60); do \
		tmux capture-pane -pJt $PANE -S -2000 | sed -n '/^$TAG\$/,\$p' | \
			grep -qE 'Please press Enter|Kernel panic' && break; sleep 2; done; \
	tmux capture-pane -pJt $PANE -S -2000 | sed -n '/^$TAG\$/,\$p' | \
		grep -E 'Starting new kernel|shut down cleanly|Bye!|Linux version|Kernel panic|Please press Enter' | head -20"
for i in $(seq 1 40); do
	$ROUTER true 2>/dev/null && break
	sleep 3
done

echo "== state"
$ROUTER 'uname -v; cat /proc/cmdline; echo "cpus online: $(cat /sys/devices/system/cpu/online)"; \
	dmesg | grep -iE "warning|oops|bug:|error|fail|timeout" | \
		grep -viE "failed to set xattr|rdinit=/init|CPU1: failed|unknown state" | head -20'
