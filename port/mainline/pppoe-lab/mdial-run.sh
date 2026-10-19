#!/bin/sh
# Load both dial sessions at once from the mac: up and down on each, 4 streams total.
#   mdial-run.sh [seconds]
T=${1:-20}
ROUTER="ssh -o BatchMode=yes -J 192.168.167.100 -p 2022 root@192.168.174.5"
ssh 192.168.167.93 "export PATH=/opt/homebrew/bin:\$PATH
	for a in 10.64.100.1 10.64.101.1; do
		iperf3 -c \$a -t $T -O 2 -J > /tmp/md-up-\$a.json 2>&1 &
		iperf3 -c \$a -p 5203 -t $T -O 2 -R -J > /tmp/md-dn-\$a.json 2>&1 &
	done" &
sleep $((T / 2 + 2))
$ROUTER 'top -bn1 | sed -n 2,3p; grep -c HW_OFFLOAD /proc/net/nf_conntrack;
	cat /sys/kernel/debug/zx279133-flows 2>/dev/null || cat /sys/kernel/debug/*/zx279133-flows;
	cat /sys/kernel/debug/zx279133-wanid 2>/dev/null || cat /sys/kernel/debug/*/zx279133-wanid'
wait
sleep $((T / 2 + 4))
ssh 192.168.167.93 'for f in /tmp/md-*.json; do printf "%s " $(basename $f .json); python3 -c "import json,sys; d=json.load(open(sys.argv[1])); print(round(d[\"end\"][\"sum_received\"][\"bits_per_second\"]/1e6), \"Mbit/s\")" $f 2>/dev/null || head -c 200 $f; done'
