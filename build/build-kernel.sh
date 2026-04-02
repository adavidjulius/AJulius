#!/usr/bin/env bash
# build/build-kernel.sh
# JULIUS OS — Build XNU kernel (DEVELOPMENT configuration)
#
# Prerequisites:
#   - Xcode + command-line tools installed
#   - git submodules initialised (kernel/xnu, kernel/dtrace, kernel/AvailabilityVersions)
#   - brew install cmake ninja python3
#
# Usage:
#   ./build/build-kernel.sh [--arch x86_64|arm64] [--config DEVELOPMENT|RELEASE]
#
# Outputs:
#   build/out/kernel.development      (bare kernel Mach-O)
#   build/out/julius.kernelcache      (prelinked kernel + essential kexts)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
XNU_DIR="$REPO_ROOT/kernel/xnu"
DTRACE_DIR="$REPO_ROOT/kernel/dtrace"
AVAIL_DIR="$REPO_ROOT/kernel/AvailabilityVersions"
SDK_STAGING="$REPO_ROOT/build/julius-sdk"
OUT_DIR="$REPO_ROOT/build/out"

OBJ_ROOT="/tmp/julius-xnu-obj"
SYM_ROOT="/tmp/julius-xnu-sym"
DST_ROOT="/tmp/julius-xnu-dst"

ARCH="x86_64"
KERNEL_CONFIG="DEVELOPMENT"

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)        ARCH="$2";          shift 2 ;;
        --config)      KERNEL_CONFIG="$2"; shift 2 ;;
        *)             echo "Unknown arg: $1"; exit 1 ;;
    esac
done

# ── Validate submodules ───────────────────────────────────────────────────────
if [[ ! -d "$XNU_DIR/osfmk" ]]; then
    echo "ERROR: XNU submodule not initialised."
    echo "Run: git submodule update --init --recursive"
    exit 1
fi

SDK="$(xcrun --sdk macosx --show-sdk-path)"
echo "==> Using SDK: $SDK"
echo "==> Building XNU: arch=$ARCH config=$KERNEL_CONFIG"

# ── Step 1: Install AvailabilityVersions headers ──────────────────────────────
echo ""
echo "--- [1/4] Installing AvailabilityVersions headers ---"
mkdir -p "$SDK_STAGING"
(cd "$AVAIL_DIR" && make install DSTROOT="$SDK_STAGING" -j"$(sysctl -n hw.ncpu)")

# ── Step 2: Build dtrace CTF tools ────────────────────────────────────────────
echo ""
echo "--- [2/4] Building dtrace CTF tools (ctfconvert, ctfmerge) ---"
DTRACE_OBJ="/tmp/julius-dtrace-obj"
mkdir -p "$DTRACE_OBJ"

# The dtrace build target for just the tools
(cd "$DTRACE_DIR" && \
 xcodebuild install \
     -target ctfconvert \
     -target ctfmerge \
     DSTROOT="$SDK_STAGING" \
     SRCROOT="$(pwd)" \
     OBJROOT="$DTRACE_OBJ" \
     SDKROOT="$SDK" \
     -quiet)

export PATH="$SDK_STAGING/usr/local/bin:$PATH"
echo "  ctfconvert: $(which ctfconvert)"

# ── Step 3: Build XNU ─────────────────────────────────────────────────────────
echo ""
echo "--- [3/4] Building XNU kernel ---"

# Determine ARCH_CONFIGS and MACHINE_CONFIG per target
case "$ARCH" in
    x86_64)
        ARCH_CONFIGS="X86_64"
        MACHINE_CONFIG="DEFAULT"
        ;;
    arm64)
        ARCH_CONFIGS="ARM64"
        MACHINE_CONFIG="VMAPPLE"   # virtualised Apple Silicon target
        ;;
    *)
        echo "Unsupported arch: $ARCH"; exit 1 ;;
esac

make -C "$XNU_DIR" \
    ARCH_CONFIGS="$ARCH_CONFIGS" \
    KERNEL_CONFIGS="$KERNEL_CONFIG" \
    MACHINE_CONFIG="$MACHINE_CONFIG" \
    OBJROOT="$OBJ_ROOT" \
    SYMROOT="$SYM_ROOT" \
    DSTROOT="$DST_ROOT" \
    SDKROOT="$SDK" \
    RC_CFLAGS="-arch $ARCH" \
    DEPENDENCIES_FILE=/dev/null \
    -j"$(sysctl -n hw.ncpu)" \
    2>&1 | tee /tmp/julius-xnu-build.log

KERNEL_BIN="$DST_ROOT/System/Library/Kernels/kernel.${KERNEL_CONFIG,,}"
if [[ ! -f "$KERNEL_BIN" ]]; then
    echo "ERROR: Kernel binary not found at $KERNEL_BIN"
    echo "Check build log: /tmp/julius-xnu-build.log"
    exit 1
fi
echo "  Kernel built: $KERNEL_BIN"

# ── Step 4: Build kernelcache ─────────────────────────────────────────────────
echo ""
echo "--- [4/4] Building kernelcache ---"
mkdir -p "$OUT_DIR"
KEXT_STAGING="/tmp/julius-kexts"
mkdir -p "$KEXT_STAGING"

# Copy the minimum kexts required to boot
SYSTEM_EXTS="/System/Library/Extensions"
ESSENTIAL_KEXTS=(
    "IOKit.kext"
    "AppleACPIPlatform.kext"
    "IOPCIFamily.kext"
    "AppleAPIC.kext"
)

echo "  Staging essential kexts..."
for kext in "${ESSENTIAL_KEXTS[@]}"; do
    if [[ -d "$SYSTEM_EXTS/$kext" ]]; then
        cp -r "$SYSTEM_EXTS/$kext" "$KEXT_STAGING/"
        echo "    + $kext"
    else
        echo "    ! $kext not found (skipping)"
    fi
done

# Also stage our custom JuliusSerial kext if built
JULIUS_SERIAL_KEXT="$OUT_DIR/JuliusSerial.kext"
if [[ -d "$JULIUS_SERIAL_KEXT" ]]; then
    cp -r "$JULIUS_SERIAL_KEXT" "$KEXT_STAGING/"
    echo "    + JuliusSerial.kext (custom)"
fi

KERNELCACHE="$OUT_DIR/julius.kernelcache"
kextcache \
    -kernel "$KERNEL_BIN" \
    -repository "$KEXT_STAGING" \
    -prelinked-kernel "$KERNELCACHE" \
    -arch "$ARCH" \
    -verbose \
    2>&1 | tee /tmp/julius-kextcache.log

# Also copy the bare kernel for debugging
cp "$KERNEL_BIN" "$OUT_DIR/kernel.${KERNEL_CONFIG,,}"

echo ""
echo "==> Build complete!"
echo ""
echo "  Kernel:      $OUT_DIR/kernel.${KERNEL_CONFIG,,}"
echo "  Kernelcache: $KERNELCACHE"
ls -lh "$OUT_DIR/"
