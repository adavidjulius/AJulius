#!/usr/bin/env bash
# build/build-kexts.sh
# JULIUS OS — Build all kernel extensions (kexts)
#
# Output: build/out/JuliusSerial.kext/
# Usage:  ./build/build-kexts.sh [--arch x86_64|arm64]

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/obj/kexts"
OUT_DIR="$REPO_ROOT/build/out"
KEXT_SRC="$REPO_ROOT/kexts"

ARCH="${1:-x86_64}"
case "$ARCH" in
    --arch) ARCH="$2" ;;
    x86_64|arm64) ;;
    *) echo "Usage: $0 [--arch x86_64|arm64]"; exit 1 ;;
esac

SDK="$(xcrun --sdk macosx --show-sdk-path)"
TARGET="${ARCH}-apple-macos13.0"

KEXT_FRAMEWORK="$SDK/System/Library/Frameworks/Kernel.framework/Headers"
if [[ ! -d "$KEXT_FRAMEWORK" ]]; then
    echo "ERROR: Kernel.framework headers not found at $KEXT_FRAMEWORK"
    echo "Ensure Xcode and the macOS SDK are installed."
    exit 1
fi

# ── Compiler flags for kext code ──────────────────────────────────────────────
# -mkernel:        enables kernel-appropriate code generation
# -fno-exceptions: kexts cannot use C++ exceptions
# -fno-rtti:       no runtime type information (reduces size, required for kexts)
# -nostdlib:       don't link standard libraries (kexts link against KPIs)

CXX_FLAGS=(
    -target "$TARGET"
    -arch "$ARCH"
    -mkernel
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -nostdinc
    -isystem "$KEXT_FRAMEWORK"
    -I "$KEXT_SRC/JuliusSerial/include"
    -O2
    -g
    -Wall -Wextra
    # Don't treat warnings as errors for kexts — IOKit headers have some
)

build_julius_serial() {
    local KEXT_NAME="JuliusSerial"
    local KEXT_OUT="$OUT_DIR/${KEXT_NAME}.kext"
    local KEXT_BIN_DIR="$KEXT_OUT/Contents/MacOS"
    local OBJ_DIR="$BUILD_DIR/$KEXT_NAME"

    echo "==> Building $KEXT_NAME.kext for $ARCH"
    mkdir -p "$OBJ_DIR" "$KEXT_BIN_DIR"

    # Compile the driver
    echo "  CXX $KEXT_SRC/$KEXT_NAME/src/${KEXT_NAME}.cpp"
    clang++ "${CXX_FLAGS[@]}" \
        -c "$KEXT_SRC/$KEXT_NAME/src/${KEXT_NAME}.cpp" \
        -o "$OBJ_DIR/${KEXT_NAME}.o"

    # Link as a kext MachO bundle
    echo "  LD  $KEXT_BIN_DIR/$KEXT_NAME"
    ld  -kext \
        -arch "$ARCH" \
        -bundle \
        -macosx_version_min 13.0 \
        -dead_strip \
        "$OBJ_DIR/${KEXT_NAME}.o" \
        -lkmod \
        -o "$KEXT_BIN_DIR/$KEXT_NAME" \
        2>/dev/null || {
            # lkmod may not be available — try without it
            ld -kext -arch "$ARCH" -bundle \
               -macosx_version_min 13.0 \
               -dead_strip \
               "$OBJ_DIR/${KEXT_NAME}.o" \
               -o "$KEXT_BIN_DIR/$KEXT_NAME"
        }

    # Copy Info.plist into the bundle
    mkdir -p "$KEXT_OUT/Contents"
    cp "$KEXT_SRC/$KEXT_NAME/Info.plist" "$KEXT_OUT/Contents/Info.plist"

    echo "  Kext bundle: $KEXT_OUT"
    echo "  Contents:"
    find "$KEXT_OUT" -type f | sort | sed 's/^/    /'
}

mkdir -p "$BUILD_DIR" "$OUT_DIR"

# Build each kext
build_julius_serial

echo ""
echo "==> All kexts built in $OUT_DIR"
