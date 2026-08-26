#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ZXROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)
OUT=${OUT:-"$ZXROOT/out"}
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
mkdir -p "$OUT"
BUILD_DIR="$OUT/.nft-static-release"
PREFIX="$BUILD_DIR/stage"
trap 'exit 1' HUP INT TERM
trap 'rm -rf "$BUILD_DIR"' EXIT

MNL_VERSION=1.0.5
MNL_SHA256=274b9b919ef3152bfb3da3a13c950dd60d6e2bcd54230ffeca298d03b40d0525
NFTNL_VERSION=1.3.1
NFTNL_SHA256=607da28dba66fbdeccf8ef1395dded9077e8d19f2995f9a4d45a9c2f0bcffba8
NFT_VERSION=1.1.6
NFT_SHA256=372931bda8556b310636a2f9020adc710f9bab66f47efe0ce90bff800ac2530c

[ ! -e "$BUILD_DIR" ] || {
	echo "build directory already exists: $BUILD_DIR" >&2
	exit 1
}

for tool in curl file make pkg-config shasum tar \
	"${CROSS_COMPILE}gcc" "${CROSS_COMPILE}ar" \
	"${CROSS_COMPILE}ranlib" "${CROSS_COMPILE}strip"; do
	command -v "$tool" >/dev/null 2>&1 || {
		echo "missing build tool: $tool" >&2
		exit 1
	}
done

mkdir -p "$PREFIX"
cd "$BUILD_DIR"

curl -fL -o "libmnl-$MNL_VERSION.tar.bz2" \
	"https://www.netfilter.org/pub/libmnl/libmnl-$MNL_VERSION.tar.bz2"
curl -fL -o "libnftnl-$NFTNL_VERSION.tar.xz" \
	"https://www.netfilter.org/pub/libnftnl/libnftnl-$NFTNL_VERSION.tar.xz"
curl -fL -o "nftables-$NFT_VERSION.tar.xz" \
	"https://www.netfilter.org/pub/nftables/nftables-$NFT_VERSION.tar.xz"

printf '%s  %s\n' "$MNL_SHA256" "libmnl-$MNL_VERSION.tar.bz2" |
	shasum -a 256 -c -
printf '%s  %s\n' "$NFTNL_SHA256" "libnftnl-$NFTNL_VERSION.tar.xz" |
	shasum -a 256 -c -
printf '%s  %s\n' "$NFT_SHA256" "nftables-$NFT_VERSION.tar.xz" |
	shasum -a 256 -c -

tar -xf "libmnl-$MNL_VERSION.tar.bz2"
tar -xf "libnftnl-$NFTNL_VERSION.tar.xz"
tar -xf "nftables-$NFT_VERSION.tar.xz"

build_library() {
	directory=$1
	(
		cd "$directory"
		CC="${CROSS_COMPILE}gcc" AR="${CROSS_COMPILE}ar" \
		RANLIB="${CROSS_COMPILE}ranlib" \
		PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" \
		./configure --quiet --host=aarch64-linux-gnu \
			--prefix="$PREFIX" --enable-static --disable-shared
		make -s -j4
		make -s install
	)
}

build_library "libmnl-$MNL_VERSION"
build_library "libnftnl-$NFTNL_VERSION"

(
	cd "nftables-$NFT_VERSION"
	CC="${CROSS_COMPILE}gcc" AR="${CROSS_COMPILE}ar" \
	RANLIB="${CROSS_COMPILE}ranlib" \
	PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" \
	CPPFLAGS="-I$PREFIX/include" LDFLAGS="-static -L$PREFIX/lib" \
	./configure --quiet --host=aarch64-linux-gnu \
		--prefix="$PREFIX" --enable-static --disable-shared \
		--with-mini-gmp --without-cli --without-json --disable-man-doc
	make -s -j4 LDFLAGS="-all-static -L$PREFIX/lib"
)

cp "nftables-$NFT_VERSION/src/nft" "$OUT/nft"
"${CROSS_COMPILE}strip" -s "$OUT/nft"
chmod 0755 "$OUT/nft"
file "$OUT/nft"
shasum -a 256 "$OUT/nft"
