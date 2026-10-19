#!/bin/sh
# catch.sh TAG: concurrent bidir soak with diagnostics at t=6s
S=/tmp/claude-1000/-home-cnjn-code-linux-mainline-zte-zxslc-sr1010/ef9dc655-f65a-4b66-add6-01efbc62d721/scratchpad
R=/tmp/claude-1000/r.sh
T=${1:-x}
ssh -o BatchMode=yes 192.168.167.93 'export PATH=/opt/homebrew/bin:$PATH; iperf3 -c 10.64.200.1 -t 20 -i 2 -f m --bidir 2>&1' > $S/c-$T-ipoe.txt 2>&1 &
ssh -o BatchMode=yes 192.168.167.93 'export PATH=/opt/homebrew/bin:$PATH; iperf3 -c 10.64.100.1 -t 20 -i 2 -f m --bidir 2>&1' > $S/c-$T-pppoe.txt 2>&1 &
sleep 6
$R 'cat /sys/kernel/debug/zx279133-flows; grep -E "users [1-9]" /sys/kernel/debug/zx279133-wanid; dmesg | grep -v "push rsp" | tail -3; /tmp/ppu-trace pc=10 steps=400 timeout=3000 match=6:00e041680d86,27:06 > /tmp/t.txt; cat /tmp/t.txt' > $S/c-$T-diag.txt 2>&1
wait
grep -E "TX-C|RX-C" $S/c-$T-ipoe.txt | tail -2
grep -E "TX-C|RX-C" $S/c-$T-pppoe.txt | tail -2
$R 'echo $(uname -v | cut -d" " -f1) $(cut -d" " -f1 /proc/uptime)' 2>&1 | tail -1
