#!/usr/bin/env bash
# build/build-all.sh
# JULIUS OS — Master build script
#
# Builds everything in the correct order:
#   1. julius-libc  (static archive)
#   2. julius-init  (PID 1 binary)
#   3. JuliusSerial kext
#   4. XNU kernel + kernelcache
#   5. Ramdisk image
#
# Usage:
#   ./build/build-all.sh [--arch x86_64|arm64] [--skip-kernel]
#
# --skip-kernel is useful during early development when you want to
# iterate on libc/init without the multi-hour kernel rebuild.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"

ARCH="x86_64"
SKIP_KERNEL=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)         ARCH="$2"; shift 2 ;;
        --skip-kernel)  SKIP_KERNEL=1; shift ;;
        *) echo "Unknown: $1"; exit 1 ;;
    esac
done

# Ensure all build scripts are executable
chmod +x "$BUILD_DIR"/*.sh

START_TIME=$SECONDS

print_step() {
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  $1"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

print_step "Step 1/5: Building julius-libc (arch=$ARCH)"
"$BUILD_DIR/build-libc.sh" --arch "$ARCH"

print_step "Step 2/5: Building julius-init (arch=$ARCH)"
"$BUILD_DIR/build-init.sh" --arch "$ARCH"

print_step "Step 3/5: Building kexts (arch=$ARCH)"
"$BUILD_DIR/build-kexts.sh" --arch "$ARCH"

if [[ "$SKIP_KERNEL" == "0" ]]; then
    print_step "Step 4/5: Building XNU kernel (arch=$ARCH) [this takes a while]"
    "$BUILD_DIR/build-kernel.sh" --arch "$ARCH"
else
    echo ""
    echo "  [Skipping kernel build — using existing kernelcache]"
fi

print_step "Step 5/5: Building ramdisk"
"$BUILD_DIR/build-ramdisk.sh" --arch "$ARCH"

ELAPSED=$(( SECONDS - START_TIME ))
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  ✓ JULIUS OS build complete in ${ELAPSED}s"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "  Artifacts in build/out/:"
ls -lh "$REPO_ROOT/build/out/" 2>/dev/null || echo "  (none yet)"
echo ""
echo "  To run in QEMU:"
if [[ "$ARCH" == "arm64" ]]; then
    echo "    ./build/run-qemu-arm64.sh"
else
    echo "    ./build/run-qemu-x86.sh"
fi
echo ""
echo "  To run with GDB/LLDB attached:"
if [[ "$ARCH" == "arm64" ]]; then
    echo "    ./build/run-qemu-arm64.sh --debug"
else
    echo "    ./build/run-qemu-x86.sh --debug"
fi
