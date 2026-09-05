#!/usr/bin/env python3

import argparse
import hashlib
import socket
import time


def report(mode: str, count: int, elapsed: float, digest: str) -> None:
    print(
        f"TCP_{mode.upper()} bytes={count} seconds={elapsed:.6f} "
        f"gbps={count * 8 / elapsed / 1e9:.6f} sha256={digest}",
        flush=True,
    )


def sink(listener: socket.socket) -> None:
    connection, peer = listener.accept()
    digest = hashlib.sha256()
    count = 0
    start = time.monotonic()
    with connection:
        while True:
            data = connection.recv(1024 * 1024)
            if not data:
                break
            digest.update(data)
            count += len(data)
    report("sink", count, time.monotonic() - start, digest.hexdigest())
    print(f"TCP_PEER address={peer[0]} port={peer[1]}", flush=True)


def source(listener: socket.socket, size: int) -> None:
    connection, peer = listener.accept()
    block = bytes(1024 * 1024)
    digest = hashlib.sha256()
    count = 0
    start = time.monotonic()
    with connection:
        while count < size:
            data = block[: min(len(block), size - count)]
            connection.sendall(data)
            digest.update(data)
            count += len(data)
        connection.shutdown(socket.SHUT_WR)
    report("source", count, time.monotonic() - start, digest.hexdigest())
    print(f"TCP_PEER address={peer[0]} port={peer[1]}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("sink", "source"))
    parser.add_argument("--bind", default="192.168.1.100")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--bytes", type=int, default=512 * 1024 * 1024)
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((args.bind, args.port))
        listener.listen(1)
        print(f"TCP_LISTEN mode={args.mode} address={args.bind} port={args.port}", flush=True)
        if args.mode == "sink":
            sink(listener)
        else:
            source(listener, args.bytes)


if __name__ == "__main__":
    main()
