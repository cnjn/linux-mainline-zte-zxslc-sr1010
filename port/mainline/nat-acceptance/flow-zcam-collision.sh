#!/bin/sh
set -eu

tc=${TC:-tc}
helper=${HELPER:-./tc-udp-flow.sh}
command=${1:?usage: flow-zcam-collision.sh add|check|del [PREF_BASE]}
pref_base=${2:-4000}

# These 32 LAN-to-WAN keys all map to ZCAM block 0, cell 0, address 0x42.
# Their reverse-direction keys likewise share block 0, cell 0, address 0x0e.
ports="10016 10298 10501 10836 11115 11494 11737 11912
12215 12460 12691 12994 13309 13424 13647 13854
14113 14395 14596 14933 15210 15591 15832 16009
16310 16555 16788 17093 17402 17527 17736 17945"

case "$command" in
add)
	added=0
	failed=0
	i=0
	for port in $ports; do
		if TC="$tc" "$helper" add "$port" $((pref_base + i)); then
			added=$((added + 1))
		else
			failed=$((failed + 1))
		fi
		i=$((i + 1))
	done
	echo "requested=$i added=$added failed=$failed"
	[ "$failed" -eq 0 ]
	;;
check)
	lan_hw=$($tc filter show dev lan-cpu0 ingress | grep -c 'in_hw_count 1')
	wan_hw=$($tc filter show dev eth0 ingress | grep -c 'in_hw_count 1')
	echo "lan_hw=$lan_hw wan_hw=$wan_hw"
	[ "$lan_hw" -eq 32 ] && [ "$wan_hw" -eq 32 ]
	;;
del)
	failed=0
	i=0
	for port in $ports; do
		TC="$tc" "$helper" del "$port" $((pref_base + i)) ||
			failed=$((failed + 1))
		i=$((i + 1))
	done
	echo "deleted=$((i - failed)) failed=$failed"
	[ "$failed" -eq 0 ]
	;;
*)
	echo "unknown command: $command" >&2
	exit 2
	;;
esac
