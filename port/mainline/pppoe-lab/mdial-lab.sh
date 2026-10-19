#!/bin/sh
# Multi-dial (two PPPoE sessions over two macvlans on eth0) on top of pppoe-lab.sh.
#   mdial-lab.sh up|down   (run pppoe-lab.sh up first)
# Targets: 10.64.100.1 via pppoe-vwan1, 10.64.101.1 via pppoe-vwan2.
set -eu
LAPTOP="ssh -o BatchMode=yes 192.168.167.100"
ROUTER="ssh -o BatchMode=yes -o ConnectTimeout=8 -J 192.168.167.100 -p 2022 root@192.168.174.5"
PKGS=/tmp/zx-pkgs

up() {
	$LAPTOP "sudo ip addr replace 10.64.101.1/32 dev zxtest0
		# One iperf3 server serves one client: 5201 for upload, 5203 for download.
		for s in 10.64.101.1:5201 10.64.100.1:5203 10.64.101.1:5203; do
			pgrep -f \"iperf3 -s -B \${s%:*} -p \${s#*:}\" >/dev/null ||
				LD_LIBRARY_PATH=$PKGS/root/usr/lib/x86_64-linux-gnu \
				$PKGS/root/usr/bin/iperf3 -s -B \${s%:*} -p \${s#*:} -D
		done"
	$ROUTER "set -e
		if ! uci -q get network.vwan1 >/dev/null; then
			for n in 1 2; do
				uci set network.mv\$n=device
				uci set network.mv\$n.type=macvlan
				uci set network.mv\$n.ifname=eth0
				uci set network.mv\$n.name=mv\$n
				uci set network.mv\$n.macaddr=7c:7d:21:b9:c1:5\$n
				uci set network.mv\$n.mode=private
				uci set network.vwan\$n=interface
				uci set network.vwan\$n.proto=pppoe
				uci set network.vwan\$n.device=mv\$n
				uci set network.vwan\$n.username=test\$n
				uci set network.vwan\$n.password=test
				uci set network.vwan\$n.service=zx279133-test
				uci set network.vwan\$n.defaultroute=0
				uci set network.vwan\$n.peerdns=0
				uci set network.vwan\$n.ipv6=0
				uci add_list firewall.@zone[1].network=vwan\$n
			done
			uci commit network; uci commit firewall
			/etc/init.d/network reload; /etc/init.d/firewall reload >/dev/null 2>&1
		fi
		for i in \$(seq 1 40); do
			ifstatus vwan1 | grep -q '\"up\": true' && ifstatus vwan2 | grep -q '\"up\": true' && break
			sleep 2
		done
		ip route replace 10.64.100.1/32 dev pppoe-vwan1
		ip route replace 10.64.101.1/32 dev pppoe-vwan2
		ip -4 -br addr show pppoe-vwan1; ip -4 -br addr show pppoe-vwan2
		nft list flowtables | grep devices"
}

down() {
	$ROUTER "if uci -q get network.vwan1 >/dev/null; then
			for n in 1 2; do
				uci delete network.vwan\$n; uci delete network.mv\$n
				uci del_list firewall.@zone[1].network=vwan\$n
			done
			uci commit network; uci commit firewall
			/etc/init.d/network reload; /etc/init.d/firewall reload >/dev/null 2>&1
		fi; echo router-clean"
	$LAPTOP "pkill -f '[i]perf3 -s -B 10.64.101.1' || true; pkill -f '[i]perf3 -s -B 10.64.100.1 -p 5203' || true; sudo ip addr del 10.64.101.1/32 dev zxtest0 2>/dev/null || true; echo laptop-clean"
}

case ${1:-} in
up) up ;;
down) down ;;
*) echo "usage: $0 up|down" >&2; exit 2 ;;
esac
