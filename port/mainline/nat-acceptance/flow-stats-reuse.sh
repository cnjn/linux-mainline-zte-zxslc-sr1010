#!/bin/sh
set -u

tc=${TC:-/tmp/tc}
helper=${HELPER:-/tmp/tc-udp-flow.sh}
cycles=${1:-100}
pref=${2:-200}
base_port=${3:-5300}

i=0
active=0
add_fail=0
hw_fail=0
zero_fail=0
del_fail=0

cleanup()
{
	[ "$active" -eq 0 ] ||
		TC="$tc" "$helper" del 0 "$pref" >/dev/null 2>&1
}
trap cleanup EXIT INT TERM

while [ "$i" -lt "$cycles" ]; do
	i=$((i + 1))
	port=$((base_port + i))
	if ! TC="$tc" "$helper" add "$port" "$pref" >/dev/null 2>&1; then
		add_fail=$((add_fail + 1))
		TC="$tc" "$helper" del "$port" "$pref" >/dev/null 2>&1
		continue
	fi
	active=1

	lan=$($tc -s filter show dev lan-cpu0 ingress)
	wan=$($tc -s filter show dev eth0 ingress)
	[ "$(printf '%s\n' "$lan" | grep -c 'in_hw_count 1')" -eq 1 ] &&
		[ "$(printf '%s\n' "$wan" | grep -c 'in_hw_count 1')" -eq 1 ] ||
		hw_fail=$((hw_fail + 1))
	[ "$(printf '%s\n' "$lan" | grep 'Sent hardware' |
		grep -vc 'Sent hardware 0 bytes 0 pkt')" -eq 0 ] &&
		[ "$(printf '%s\n' "$wan" | grep 'Sent hardware' |
		grep -vc 'Sent hardware 0 bytes 0 pkt')" -eq 0 ] ||
		zero_fail=$((zero_fail + 1))

	if ! TC="$tc" "$helper" del "$port" "$pref" >/dev/null 2>&1; then
		del_fail=$((del_fail + 1))
	else
		active=0
	fi
	if [ $((i % 10)) -eq 0 ]; then
		echo "progress=$i add_fail=$add_fail hw_fail=$hw_fail zero_fail=$zero_fail del_fail=$del_fail"
	fi
done

cleanup
active=0
lan_left=$($tc filter show dev lan-cpu0 ingress | grep -c "pref $pref")
wan_left=$($tc filter show dev eth0 ingress | grep -c "pref $pref")
echo "cycles=$i add_fail=$add_fail hw_fail=$hw_fail zero_fail=$zero_fail del_fail=$del_fail lan_left=$lan_left wan_left=$wan_left"

[ "$add_fail" -eq 0 ] && [ "$hw_fail" -eq 0 ] &&
	[ "$zero_fail" -eq 0 ] && [ "$del_fail" -eq 0 ] &&
	[ "$lan_left" -eq 0 ] && [ "$wan_left" -eq 0 ]
