#!/bin/sh
# soak.sh N: N rounds of concurrent IPoE + PPPoE bidirectional iperf3, router uptime after each
R=/tmp/claude-1000/r.sh
for n in $(seq 1 ${1:-6}); do
	$R '(i=0; while [ $i -lt 30 ]; do echo "HB $(cut -d" " -f1 /proc/uptime)" > /dev/console; sleep 1; i=$((i+1)); done) </dev/null >/dev/null 2>&1 &' 2>/dev/null
	out=$(ssh -o BatchMode=yes 192.168.167.93 'export PATH=/opt/homebrew/bin:$PATH; iperf3 -c 10.64.200.1 -t 20 -f m --bidir 2>&1 | grep -E "receiver|error" | tr -s " " | cut -d" " -f3,7,8 | tr "\n" " " & iperf3 -c 10.64.100.1 -t 20 -f m --bidir 2>&1 | grep -E "receiver|error" | tr -s " " | cut -d" " -f3,7,8 | tr "\n" " "; wait')
	up=$($R 'echo $(uname -v | cut -d" " -f1) $(cut -d" " -f1 /proc/uptime)' 2>&1 | tail -1)
	echo "round $n: $out | $up"
done
