#!/bin/bash
# Second WAN on a LAN port: the laptop cable moves from the WAN port to lan4.
#   [WAN2_PORT=lan3] wan2-lab.sh up|down   (default lan4)
# lan4 leaves br-lan and becomes "wan2" (static 192.168.175.5/24) plus
# "wan2ppp" (PPPoE over lan4), both in the wan zone.
# Targets: 10.64.200.1 via wan2 (IPoE), 10.64.101.1 via pppoe-wan2ppp.
# The router is reached over its LAN side (mac on lan2), so moving the cable
# does not cut access.
set -eu
LAPTOP="ssh -o BatchMode=yes 192.168.167.100"
# The mac's shell parses the command once more, so quote it for that.
ROUTER() {
	ssh -o BatchMode=yes 192.168.167.93 \
		"ssh -o BatchMode=yes -o ConnectTimeout=8 root@192.168.5.1 $(printf %q "$1")"
}
PKGS=/tmp/zx-pkgs
P=${WAN2_PORT:-lan4}

up() {
	$LAPTOP "sudo ip addr replace 192.168.175.254/24 dev enp2s0
		sudo ip addr replace 10.64.101.1/32 dev zxtest0
		for s in 10.64.200.1:5203 10.64.101.1:5201 10.64.101.1:5203; do
			pgrep -f \"iperf3 -s -B \${s%:*} -p \${s#*:}\" >/dev/null ||
				LD_LIBRARY_PATH=$PKGS/root/usr/lib/x86_64-linux-gnu \
				$PKGS/root/usr/bin/iperf3 -s -B \${s%:*} -p \${s#*:} -D
		done"
	ROUTER "set -e
		if ! uci -q get network.wan2 >/dev/null; then
			uci del_list network.@device[0].ports=$P
			uci set network.wan2=interface
			uci set network.wan2.proto=static
			uci set network.wan2.device=$P
			uci set network.wan2.ipaddr=192.168.175.5
			uci set network.wan2.netmask=255.255.255.0
			uci set network.wan2ppp=interface
			uci set network.wan2ppp.proto=pppoe
			uci set network.wan2ppp.device=$P
			uci set network.wan2ppp.username=test3
			uci set network.wan2ppp.password=test
			uci set network.wan2ppp.service=zx279133-test
			uci set network.wan2ppp.defaultroute=0
			uci set network.wan2ppp.peerdns=0
			uci set network.wan2ppp.ipv6=0
			uci add_list firewall.@zone[1].network=wan2
			uci add_list firewall.@zone[1].network=wan2ppp
			uci commit network; uci commit firewall
			/etc/init.d/network reload; /etc/init.d/firewall reload >/dev/null 2>&1
		fi
		for i in \$(seq 1 40); do
			ifstatus wan2ppp | grep -q '\"up\": true' && break
			sleep 2
		done
		ip route replace 10.64.200.1/32 via 192.168.175.254 dev $P
		ip route replace 10.64.101.1/32 dev pppoe-wan2ppp || true
		ip -4 -br addr show $P; ip -4 -br addr show pppoe-wan2ppp || true
		nft list flowtables | grep devices"
}

down() {
	ROUTER "if uci -q get network.wan2 >/dev/null; then
			uci delete network.wan2; uci delete network.wan2ppp
			uci del_list firewall.@zone[1].network=wan2
			uci del_list firewall.@zone[1].network=wan2ppp
			uci add_list network.@device[0].ports=$P
			uci commit network; uci commit firewall
			/etc/init.d/network reload; /etc/init.d/firewall reload >/dev/null 2>&1
		fi; echo router-clean"
	$LAPTOP "pkill -f '[i]perf3 -s -B 10.64.200.1 -p 5203' || true
		pkill -f '[i]perf3 -s -B 10.64.101.1' || true
		sudo ip addr del 10.64.101.1/32 dev zxtest0 2>/dev/null || true
		sudo ip addr del 192.168.175.254/24 dev enp2s0 2>/dev/null || true
		echo laptop-clean"
}

case ${1:-} in
up) up ;;
down) down ;;
*) echo "usage: $0 up|down" >&2; exit 2 ;;
esac
