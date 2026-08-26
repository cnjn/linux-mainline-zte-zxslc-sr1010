#!/bin/sh
set -u

tc=${TC:-/tmp/tc}
helper=${HELPER:-/tmp/tc-udp-flow.sh}
command=${1:?usage: flow-stats-capacity.sh add|check|del COUNT [PREF_BASE] [PORT_BASE]}
count=${2:?usage: flow-stats-capacity.sh add|check|del COUNT [PREF_BASE] [PORT_BASE]}
pref_base=${3:-1000}
port_base=${4:-6000}

case "$command" in
add)
	i=0
	added=0
	failed=0
	while [ "$i" -lt "$count" ]; do
		pref=$((pref_base + i))
		port=$((port_base + i))
		if TC="$tc" "$helper" add "$port" "$pref" >/dev/null 2>&1; then
			added=$((added + 1))
		else
			failed=$((failed + 1))
			echo "add_failed index=$i port=$port pref=$pref"
			TC="$tc" "$helper" del "$port" "$pref" >/dev/null 2>&1
		fi
		i=$((i + 1))
		[ $((i % 64)) -ne 0 ] ||
			echo "progress=$i added=$added failed=$failed"
	done
	echo "requested=$count added=$added failed=$failed"
	[ "$failed" -eq 0 ]
	;;
check)
	lan=$($tc -s filter show dev lan-cpu0 ingress)
	wan=$($tc -s filter show dev eth0 ingress)
	lan_hw=$(printf '%s\n' "$lan" | grep -c 'in_hw_count 1')
	wan_hw=$(printf '%s\n' "$wan" | grep -c 'in_hw_count 1')
	lan_nonzero=$(printf '%s\n' "$lan" | grep 'Sent hardware' |
		grep -vc 'Sent hardware 0 bytes 0 pkt')
	wan_nonzero=$(printf '%s\n' "$wan" | grep 'Sent hardware' |
		grep -vc 'Sent hardware 0 bytes 0 pkt')
	echo "lan_hw=$lan_hw wan_hw=$wan_hw lan_nonzero=$lan_nonzero wan_nonzero=$wan_nonzero"
	[ "$lan_hw" -eq "$count" ] && [ "$wan_hw" -eq "$count" ]
	;;
del)
	i=0
	failed=0
	while [ "$i" -lt "$count" ]; do
		pref=$((pref_base + i))
		port=$((port_base + i))
		TC="$tc" "$helper" del "$port" "$pref" >/dev/null 2>&1 ||
			failed=$((failed + 1))
		i=$((i + 1))
	done
	lan_left=$($tc filter show dev lan-cpu0 ingress | grep -c 'in_hw_count 1')
	wan_left=$($tc filter show dev eth0 ingress | grep -c 'in_hw_count 1')
	echo "deleted=$count failed=$failed lan_left=$lan_left wan_left=$wan_left"
	[ "$failed" -eq 0 ] && [ "$lan_left" -eq 0 ] && [ "$wan_left" -eq 0 ]
	;;
*)
	echo "unknown command: $command" >&2
	exit 2
	;;
esac
