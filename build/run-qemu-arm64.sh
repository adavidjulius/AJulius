#!/usr/bin/env bash
# build/run-qemu-arm64.sh
# JULIUS OS — Run in QEMU (ARM64 / virt machine)
#
# The "virt" machine provides:
#   - Cortex-A57 (or newer) CPU
#   - PL011 UART at 0x09000000 (matches JuliusSerial default base)
#   - GIC interrupt controller
#   - VirtIO block device
#   - 1+ GiB of RAM starting at 0x40000000
#
# This is the development target that mirrors the ARM64 custom silicon path.
#
# Usage:
#   ./build/run-qemu-arm64.sh [--debug] [--cpu cortex-a72]

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/build/out"

KERNELCACHE="$OUT_DIR/julius.kernelcache"    # ARM64 build
RAMDISK="$OUT_DIR/julius-ramdisk.dmg"
DISK_IMG="$OUT_DIR/julius-disk-arm64.img"

DEBUG_MODE=0
CPU_MODEL="cortex-a57"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug) DEBUG_MODE=1; shift ;;
        --cpu)   CPU_MODEL="$2"; shift 2 ;;
        *) echo "Unknown: $1"; exit 1 ;;
    esac
done

if [[ ! -f "$KERNELCACHE" ]]; then
    echo "ERROR: ARM64 kernelcache not found: $KERNELCACHE"
    echo "Run: ./build/build-kernel.sh --arch arm64"
    exit 1
fi

if [[ ! -f "$DISK_IMG" ]]; then
    qemu-img create -f raw "$DISK_IMG" 64M
fi

# ── Boot arguments for ARM64 XNU ─────────────────────────────────────────────
# serial=3       PL011 UART (ARM serial device index 3 in XNU's table)
# earlycon       Enable early console output before drivers load
# debug=0x14e    Kernel debugger
# -v -x          Verbose + safe mode

BOOT_ARGS="serial=3 earlycon -v -x keepsyms=1 debug=0x14e"
if [[ -f "$RAMDISK" ]]; then
    BOOT_ARGS="$BOOT_ARGS rd=md0 init=/sbin/init"
fi

QEMU_ARGS=(
    -M "virt,highmem=off"          # virt machine, no high memory (compat)
    -cpu "$CPU_MODEL"
    -m 1024
    -kernel "$KERNELCACHE"
    -append "$BOOT_ARGS"
    -serial stdio
    -nographic
    -no-reboot
    -d guest_errors
    # PL011 UART is built into the virt machine at 0x09000000 — no extra flags needed
)

if [[ -f "$RAMDISK" ]]; then
    QEMU_ARGS+=(
        -drive "file=$RAMDISK,format=raw,if=none,id=ramdisk,readonly=on"
        -device "virtio-blk-device,drive=ramdisk"
    )
fi

QEMU_ARGS+=(
    -drive "file=$DISK_IMG,format=raw,if=none,id=hd0"
    -device "virtio-blk-device,drive=hd0"
)

if [[ "$DEBUG_MODE" == "1" ]]; then
    QEMU_ARGS+=(-s -S)
    echo "  *** DEBUG MODE on :1234 ***"
fi

echo "==> Launching JULIUS OS in QEMU (ARM64 virt)"
echo "    CPU: $CPU_MODEL | Boot args: $BOOT_ARGS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

exec qemu-system-aarch64 "${QEMU_ARGS[@]}"
