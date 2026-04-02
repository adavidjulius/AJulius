#!/usr/bin/env bash
# build/build-libc.sh
# JULIUS OS — Build julius-libc (static archive)
#
# Output: build/out/libjulius.a
# Usage:  ./build/build-libc.sh [--arch x86_64|arm64]

set -euo pipefail

# ── Configuration ────────────────────────────────────────────────────────────
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/obj/libc"
OUT_DIR="$REPO_ROOT/build/out"
SRC_DIR="$REPO_ROOT/libc/src"
INC_DIR="$REPO_ROOT/libc/include"

ARCH="${1:-x86_64}"
case "$ARCH" in
    --arch) ARCH="$2" ;;
    x86_64|arm64) ;;
    *) echo "Usage: $0 [--arch x86_64|arm64]"; exit 1 ;;
esac

# Detect macOS SDK
SDK="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null)" || {
    echo "ERROR: Cannot find macOS SDK. Install Xcode command-line tools."
    exit 1
}

TARGET="${ARCH}-apple-macos13.0"

CFLAGS=(
    -target "$TARGET"
    -arch "$ARCH"
    -ffreestanding        # no host libc headers
    -fno-builtin          # don't substitute our calls with compiler builtins
    -fno-stack-protector  # no __stack_chk_guard (we don't have it)
    -mno-red-zone         # required for kernel-adjacent freestanding code
    -nostdinc             # don't search system include paths
    -I "$INC_DIR"
    -O2
    -g
    -Wall
    -Wextra
    -Werror
)

# ── Setup ─────────────────────────────────────────────────────────────────────
mkdir -p "$BUILD_DIR" "$OUT_DIR"
OBJECTS=()

echo "==> Building julius-libc for $ARCH"

# ── Compile C sources ─────────────────────────────────────────────────────────
for src in "$SRC_DIR"/*.c; do
    base="$(basename "$src" .c)"
    obj="$BUILD_DIR/${base}.o"
    echo "  CC $src"
    clang "${CFLAGS[@]}" -c "$src" -o "$obj"
    OBJECTS+=("$obj")
done

# ── Archive ───────────────────────────────────────────────────────────────────
OUT="$OUT_DIR/libjulius_${ARCH}.a"
echo "  AR $OUT"
ar rcs "$OUT" "${OBJECTS[@]}"

echo "==> julius-libc built: $OUT"
echo "    Objects: ${#OBJECTS[@]}"
ls -lh "$OUT"
