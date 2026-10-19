#!/bin/bash
# mcbench.sh MBPS SECS [LEN]: laptop (PPPoE peer 10.64.0.1) streams 239.1.1.1:5000
# through the router's WAN PPPoE to the mac on lan2 (en8); prints router CPU
# (from /proc/stat deltas), softirq split and receiver loss.
R=${1:-100} S=${2:-20} L=${3:-1316}
G=${G:-239.1.1.1} P=${P:-5000}
RL=/tmp/claude-1000/rl.sh
MACIP=${MACIP:-192.168.5.240}

# An IPv6 group (G contains ':') uses the IPv6 tools, sending from SRC6 on the
# laptop's PPPoE link. mcrep sends the reports with Router Alert (omcproxy wants it).
case $G in
*:*)	RECV="/tmp/mcrecv6 $G $P en8 $((S + 8))"
	SEND="/tmp/mcsend6 ${DST:-$G} $P ${SRC6:-2001:db8:64::1} ${PPPIF:-ppp0} $R $S $L" ;;
*)	RECV="/tmp/mcrecv $G $P en8 $MACIP $((S + 8))"
	SEND="/tmp/mcsend ${DST:-$G} $P ${SRC:-10.64.0.1} $R $S $L" ;;
esac
ssh 192.168.167.93 "sudo -n /tmp/mcrep $G en8 $((S + 12)) >/dev/null 2>&1 &
	sudo -n $RECV > /tmp/mcrecv.out 2>&1 &" &
sleep 4
# Sample the middle of the stream so ramp-up and tail don't count.
$RL "rm -f /tmp/mb.soak /tmp/mb.1 /tmp/mb.2"
[ -n "$NOSOAK" ] || $RL "(sleep 4; (/tmp/soak 0 $((S - 8)) & /tmp/soak 1 $((S - 8)); wait) > /tmp/mb.soak) </dev/null >/dev/null 2>&1 &"
$RL "(sleep 4; cat /proc/uptime /proc/stat /proc/softirqs > /tmp/mb.1; cat /proc/net/dev >> /tmp/mb.1;
	sleep $((S - 8)); cat /proc/uptime /proc/stat /proc/softirqs > /tmp/mb.2; cat /proc/net/dev >> /tmp/mb.2) >/dev/null 2>&1 &"
ssh 192.168.167.100 "$SEND"
sleep 6
wait
ssh 192.168.167.93 'cat /tmp/mcrecv.out'
$RL 'cat /tmp/mb.soak 2>/dev/null; echo ===; cat /tmp/mb.1; echo ===; cat /tmp/mb.2' | python3 -c '
import sys
sk, a, b = sys.stdin.read().split("===\n")
base = {"cpu0": 248e6, "cpu1": 248e6}
for x in sk.split("\n"):
	if x.strip():
		c, n = x.split(); print("%s real busy %5.1f%% (soak)" % (c, 100 * (1 - float(n) / base[c])))
def parse(t):
	l = t.splitlines()
	up = float(l[0].split()[0]); cpu = {}; sirq = {}; dev = {}
	for x in l[1:]:
		f = x.split()
		if not f: continue
		if f[0].startswith("cpu"): cpu[f[0]] = list(map(int, f[1:]))
		elif f[0].endswith(":") and len(f) > 2 and f[1].isdigit() and f[0][:-1].isupper(): sirq[f[0][:-1]] = list(map(int, f[1:]))
		elif ":" in x and "|" not in x and not f[0][:-1].isupper():
			n, r = x.split(":", 1); r = r.split()
			if len(r) >= 16: dev[n.strip()] = (int(r[1]), int(r[9]), int(r[2]) + int(r[3]))
	return up, cpu, sirq, dev
u1, c1, s1, d1 = parse(a); u2, c2, s2, d2 = parse(b); dt = u2 - u1
for k in sorted(c1):
	d = [y - x for x, y in zip(c1[k], c2[k])]; tot = sum(d) or 1
	print("%-5s busy %5.1f%%  usr %4.1f sys %4.1f irq %4.1f sirq %4.1f" % (k, 100 * (tot - d[3] - d[4]) / tot, 100 * d[0] / tot, 100 * d[2] / tot, 100 * d[5] / tot, 100 * d[6] / tot))
print("softirq/s:", "  ".join("%s %s" % (k, "/".join(str(int((y - x) / dt)) for x, y in zip(s1[k], s2[k]))) for k in ("NET_RX", "NET_TX", "TIMER", "SCHED", "RCU") if k in s1))
for n in ("eth0", "pppoe-wanppp", "br-lan", "lan2"):
	if n in d1:
		x, y = d1[n], d2[n]
		print("%-13s rx %6d pps  tx %6d pps  drop %d" % (n, (y[0] - x[0]) / dt, (y[1] - x[1]) / dt, y[2] - x[2]))
'
