#!/bin/sh
# PPPoE / IPoE test fixture for the SR1010, with the router WAN cabled to the
# Ubuntu laptop (see docs/mainline-network/WANID_PROFILES.md, section 6).
#
#   pppoe-lab.sh up     set everything up; safe to run again, e.g. after a
#                       kexec or a reboot of the router
#   pppoe-lab.sh down   remove what "up" added (the laptop stays the
#                       router's IPoE upstream: that part is not touched)
#
# Targets on the laptop, each with an iperf3 server on port 5201:
#   10.64.200.1      reached over IPoE (router default route)
#   10.64.100.1      reached over PPPoE (router route via pppoe-wanppp)
#   192.168.174.254  the laptop's own WAN-side address (IPoE)
# The mac (LAN side, en8) routes 10.64.0.0/16 and 192.168.174.254 through the
# router. Everything on the laptop and the mac is runtime state only.
set -eu

LAPTOP="ssh -o BatchMode=yes 192.168.167.100"
ROUTER="ssh -o BatchMode=yes -o ConnectTimeout=8 -J 192.168.167.100 -p 2022 root@192.168.174.5"
MAC="ssh -o BatchMode=yes 192.168.167.93"
PKGS=/tmp/zx-pkgs

up() {
	echo "== laptop"
	$LAPTOP "set -e
		if [ ! -x $PKGS/root/usr/sbin/pppoe-server ]; then
			mkdir -p $PKGS && cd $PKGS
			apt-get download pppoe iperf3 libiperf0 libsctp1 >/dev/null
			for d in *.deb; do dpkg -x \$d root; done
		fi
		sudo ip link add zxtest0 type dummy 2>/dev/null || true
		sudo ip link set zxtest0 up
		for a in 10.64.100.1 10.64.200.1; do
			sudo ip addr replace \$a/32 dev zxtest0
		done
		sudo ip route replace 10.64.1.0/24 via 192.168.174.5 dev enp2s0
		printf 'noauth\nms-dns 223.5.5.5\nlcp-echo-interval 10\nlcp-echo-failure 6\nmtu 1492\nmru 1492\n' > /tmp/zx-pppoe-options
		pgrep -x pppoe-server >/dev/null ||
			sudo $PKGS/root/usr/sbin/pppoe-server -I enp2s0 -k -q /usr/sbin/pppd \
				-L 10.64.0.1 -R 10.64.0.2 -N 4 -C zx279133-test -S zx279133-test \
				-O /tmp/zx-pppoe-options -X /tmp/zx-pppoe-server.pid
		for a in 10.64.100.1 10.64.200.1 192.168.174.254; do
			pgrep -f \"iperf3 -s -B \$a \" >/dev/null ||
				LD_LIBRARY_PATH=$PKGS/root/usr/lib/x86_64-linux-gnu \
				$PKGS/root/usr/bin/iperf3 -s -B \$a -p 5201 -D
		done
		pgrep -fa 'pppoe-server|iperf3 -s' | cut -c1-80"

	echo "== router"
	# Files under /root do not survive a reboot on this router; /etc does.
	$ROUTER "set -e
		[ -d /etc/zx-backup ] || { mkdir /etc/zx-backup; cp /etc/config/network /etc/config/firewall /etc/zx-backup/; }
		if ! uci -q get network.wanppp >/dev/null; then
			uci set network.wanppp=interface
			uci set network.wanppp.proto=pppoe
			uci set network.wanppp.device=eth0
			uci set network.wanppp.username=test
			uci set network.wanppp.password=test
			uci set network.wanppp.service=zx279133-test
			uci set network.wanppp.defaultroute=0
			uci set network.wanppp.peerdns=0
			uci set network.wanppp.ipv6=0
			uci add_list firewall.@zone[1].network=wanppp
			uci commit network; uci commit firewall
			/etc/init.d/network reload; /etc/init.d/firewall reload >/dev/null 2>&1
		fi
		for i in \$(seq 1 30); do
			ifstatus wanppp | grep -q '\"up\": true' && break; sleep 2
		done
		ip route replace 10.64.100.0/24 dev pppoe-wanppp
		ip -4 -br addr show pppoe-wanppp"

	echo "== mac"
	# Pin the routes to en8 (2.5G, lan2): with en0 (1G, lan1) also on the
	# router LAN, macOS may pick en0 and cap every test at 1G.
	$MAC "sudo route -n add -net 10.64.0.0/16 192.168.5.1 -ifp en8 >/dev/null 2>&1 ||
			sudo route -n change -net 10.64.0.0/16 192.168.5.1 -ifp en8 >/dev/null 2>&1 || true
		sudo route -n add -host 192.168.174.254 192.168.5.1 -ifp en8 >/dev/null 2>&1 ||
			sudo route -n change -host 192.168.174.254 192.168.5.1 -ifp en8 >/dev/null 2>&1 || true
		ping -c 1 -t 3 10.64.100.1 | tail -1; ping -c 1 -t 3 10.64.200.1 | tail -1"
}

down() {
	echo "== mac"
	$MAC "sudo route -n delete -net 10.64.0.0/16 >/dev/null 2>&1 || true
		sudo route -n delete -host 192.168.174.254 >/dev/null 2>&1 || true"
	echo "== router"
	$ROUTER "if uci -q get network.wanppp >/dev/null; then
			uci delete network.wanppp
			uci del_list firewall.@zone[1].network=wanppp
			uci commit network; uci commit firewall
			/etc/init.d/network reload; /etc/init.d/firewall reload >/dev/null 2>&1
		fi
		uci get firewall.@zone[1].network"
	echo "== laptop"
	# Bracketed patterns, so pkill does not match this remote shell itself.
	$LAPTOP "sudo pkill -f '[p]ppoe-server -I enp2s0' || true
		sudo pkill pppd || true
		pkill -f 'zx-pkgs/root/usr/bin/[i]perf3' || true
		sudo ip link del zxtest0 2>/dev/null || true
		sudo ip route del 10.64.1.0/24 2>/dev/null || true
		pgrep -fa 'pppoe-server|pppd|iperf3' || echo stopped"
}

case ${1:-} in
up) up ;;
down) down ;;
*) echo "usage: $0 up|down" >&2; exit 2 ;;
esac
