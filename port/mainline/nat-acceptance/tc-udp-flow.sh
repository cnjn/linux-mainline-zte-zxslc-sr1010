#!/bin/sh
set -eu

tc=${TC:-tc}
command=${1:?usage: tc-udp-flow.sh add|del PORT PREF}
port=${2:?usage: tc-udp-flow.sh add|del PORT PREF}
pref=${3:?usage: tc-udp-flow.sh add|del PORT PREF}

lan_ingress=lan-cpu0
lan_egress=lan1
wan=eth0
lan_ip=192.168.5.100
router_wan_ip=192.168.1.1
wan_peer_ip=192.168.1.100
lan_mac=f8:89:3c:26:fe:02
wan_peer_mac=00:e0:41:68:0d:86

if [ "$command" = del ]; then
	$tc filter del dev "$lan_ingress" ingress protocol ip pref "$pref"
	$tc filter del dev "$wan" ingress protocol ip pref "$pref"
	exit
fi

if [ "$command" != add ]; then
	echo "unknown command: $command" >&2
	exit 2
fi

router_mac=$(cat /sys/class/net/$wan/address)

$tc qdisc show dev "$lan_ingress" | grep -q clsact ||
	$tc qdisc add dev "$lan_ingress" clsact
$tc qdisc show dev "$wan" | grep -q clsact ||
	$tc qdisc add dev "$wan" clsact

$tc filter replace dev "$lan_ingress" ingress protocol ip pref "$pref" \
	flower skip_sw ip_proto udp \
	src_ip "$lan_ip" dst_ip "$wan_peer_ip" \
	src_port "$port" dst_port "$port" \
	action pedit ex munge eth src set "$router_mac" \
	action pedit ex munge eth dst set "$wan_peer_mac" \
	action pedit ex munge ip src set "$router_wan_ip" \
	action csum ip and udp \
	action mirred egress redirect dev "$wan"

$tc filter replace dev "$wan" ingress protocol ip pref "$pref" \
	flower skip_sw ip_proto udp \
	src_ip "$wan_peer_ip" dst_ip "$router_wan_ip" \
	src_port "$port" dst_port "$port" \
	action pedit ex munge eth src set "$router_mac" \
	action pedit ex munge eth dst set "$lan_mac" \
	action pedit ex munge ip dst set "$lan_ip" \
	action csum ip and udp \
	action mirred egress redirect dev "$lan_egress"
