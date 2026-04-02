#!/usr/bin/env bash
# build/build-init.sh
# JULIUS OS — Build julius-init (PID 1 static binary)
#
# Depends on build-libc.sh having run first.
# Output: build/out/julius-init
# Usage:  ./build/build-init.sh [--arch x86_64|arm64]

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/obj/init"
OUT_DIR="$REPO_ROOT/build/out"
SRC_DIR="$REPO_ROOT/init/src"
INC_DIR="$REPO_ROOT/libc/include"

ARCH="${1:-x86_64}"
case "$ARCH" in
    --arch) ARCH="$2" ;;
    x86_64|arm64) ;;
    *) echo "Usage: $0 [--arch x86_64|arm64]"; exit 1 ;;
esac

LIBC="$OUT_DIR/libjulius_${ARCH}.a"
if [[ ! -f "$LIBC" ]]; then
    echo "==> libjulius_${ARCH}.a not found. Building libc first..."
    "$REPO_ROOT/build/build-libc.sh" --arch "$ARCH"
fi

TARGET="${ARCH}-apple-macos13.0"

CFLAGS=(
    -target "$TARGET"
    -arch "$ARCH"
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -mno-red-zone
    -nostdinc
    -I "$INC_DIR"
    -O2
    -g
    -Wall -Wextra -Werror
)

mkdir -p "$BUILD_DIR" "$OUT_DIR"

echo "==> Building julius-init for $ARCH"

# ── Assemble start.s ──────────────────────────────────────────────────────────
echo "  AS $SRC_DIR/start.s"
clang -target "$TARGET" -arch "$ARCH" \
      -c "$SRC_DIR/start.s" \
      -o "$BUILD_DIR/start.o"

# ── Compile main.c ────────────────────────────────────────────────────────────
echo "  CC $SRC_DIR/main.c"
clang "${CFLAGS[@]}" -c "$SRC_DIR/main.c" -o "$BUILD_DIR/main.o"

# ── Link ──────────────────────────────────────────────────────────────────────
OUT="$OUT_DIR/julius-init_${ARCH}"
echo "  LD $OUT"

ld  -static \
    -arch "$ARCH" \
    -macosx_version_min 13.0 \
    -e _start \
    -no_uuid \
    -dead_strip \
    "$BUILD_DIR/start.o" \
    "$BUILD_DIR/main.o" \
    "$LIBC" \
    -o "$OUT"

echo "==> julius-init built: $OUT"
echo ""
echo "  Verification:"
file "$OUT"
echo ""
echo "  Size:"
ls -lh "$OUT"
echo ""
echo "  Dynamic dependencies (should be empty for static binary):"
otool -L "$OUT" 2>/dev/null || echo "  (none)"
echo ""
echo "  Entry point:"
nm "$OUT" | grep " _start"
