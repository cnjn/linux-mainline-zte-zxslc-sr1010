#!/bin/sh
# Stage the Alpine arm64 pppd runtime consumed by build-zxdbg.sh.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ZXROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)
OUT=${OUT:-"$ZXROOT/out"}
PPPD_ROOT=${PPPD_ROOT:-"$OUT/ppp-root"}
IMAGE=${IMAGE:-alpine:3.24}

command -v docker >/dev/null 2>&1 || {
	echo "docker not found" >&2
	exit 1
}
mkdir -p "$PPPD_ROOT"

docker run --rm --platform linux/arm64 "$IMAGE" sh -ec '
	apk add --no-cache ppp-daemon ppp-pppoe >/dev/null
	tar -C / -chf - \
		lib/ld-musl-aarch64.so.1 \
		usr/sbin/pppd \
		usr/lib/pppd/2.5.3/pppoe.so \
		usr/lib/libpcap.so.1 \
		usr/lib/libssl.so.3 \
		usr/lib/libcrypto.so.3 \
		usr/lib/ossl-modules/legacy.so
' | tar -xf - -C "$PPPD_ROOT"

echo "pppd runtime: $PPPD_ROOT"
