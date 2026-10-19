#!/usr/bin/env python3
# udp-blast.py server BIND PORT COUNT   - wait for one datagram, answer with COUNT datagrams
# udp-blast.py client SRC DST PORT SECS - send a probe every 50 ms, count answers
import socket, sys, time
mode = sys.argv[1]
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
if mode == "server":
    s.bind((sys.argv[2], int(sys.argv[3])))
    _, peer = s.recvfrom(2048)
    for i in range(int(sys.argv[4])):
        s.sendto(b"x" * 1000, peer)
        time.sleep(0.001)
else:
    s.bind((sys.argv[2], 6001))
    s.settimeout(0.05)
    end, got = time.time() + float(sys.argv[5]), 0
    while time.time() < end:
        s.sendto(b"p", (sys.argv[3], int(sys.argv[4])))
        try:
            while True:
                s.recvfrom(2048)
                got += 1
        except socket.timeout:
            pass
    print("received", got)
