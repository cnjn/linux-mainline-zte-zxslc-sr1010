#!/bin/sh
# cold.sh: on a freshly cold-booted test firmware, deploy helpers, PPPoE lab, PC10, soak
S=/tmp/claude-1000/-home-cnjn-code-linux-mainline-zte-zxslc-sr1010/ef9dc655-f65a-4b66-add6-01efbc62d721/scratchpad
R=/tmp/claude-1000/r.sh
until $R true 2>/dev/null; do sleep 3; done
ssh -p 2022 192.168.223.19 'cat ~/workspace/ppu-trace' | $R 'cat > /tmp/ppu-trace; chmod +x /tmp/ppu-trace'
cat $S/pc10-on.sh | $R 'cat > /tmp/pc10-on.sh'
cat $S/pc10-off.sh | $R 'cat > /tmp/pc10-off.sh'
cat $S/sdt50-lan.sh | $R 'cat > /tmp/sdt50-lan.sh'
timeout 250 ~/zx279133-docs/port/mainline/pppoe-lab/pppoe-lab.sh up >/dev/null 2>&1
$R 'uname -v; cut -d" " -f1 /proc/uptime; cat /sys/devices/system/cpu/online; D=/sys/kernel/debug; echo Y > $D/zx279133-lan-pc10; sh /tmp/pc10-on.sh >/dev/null; sh /tmp/sdt50-lan.sh >/dev/null; dmesg -n 5; ip -4 addr show pppoe-wanppp | grep inet'
sh $S/soak.sh ${1:-8}
