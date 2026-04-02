#!/usr/bin/env bash
# build/run-qemu-x86.sh
# JULIUS OS — Run in QEMU (x86_64 / q35)
#
# Requires:
#   brew install qemu
#   build/out/julius.kernelcache  (from build-kernel.sh)
#   build/out/julius-ramdisk.dmg (from build-ramdisk.sh)
#
# Usage:
#   ./build/run-qemu-x86.sh [--debug]      # --debug pauses for GDB/LLDB on :1234
#   ./build/run-qemu-x86.sh [--novga]      # headless serial only

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/build/out"

KERNELCACHE="$OUT_DIR/julius.kernelcache"
RAMDISK="$OUT_DIR/julius-ramdisk.dmg"
DISK_IMG="$OUT_DIR/julius-disk.img"

DEBUG_MODE=0
NOVGA=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)  DEBUG_MODE=1; shift ;;
        --novga)  NOVGA=1;      shift ;;
        *) echo "Unknown: $1"; exit 1 ;;
    esac
done

# ── Validate artifacts ────────────────────────────────────────────────────────
if [[ ! -f "$KERNELCACHE" ]]; then
    echo "ERROR: Kernelcache not found: $KERNELCACHE"
    echo "Run: ./build/build-kernel.sh"
    exit 1
fi

# Create a blank disk image if needed (QEMU needs a block device)
if [[ ! -f "$DISK_IMG" ]]; then
    echo "Creating blank disk image..."
    qemu-img create -f raw "$DISK_IMG" 64M
fi

# ── Boot arguments ────────────────────────────────────────────────────────────
#
# serial=2       Route kprintf to COM1 (IRQ 4, port 0x3F8)
# -v             Verbose mode: all kprintf visible on console
# -x             Safe mode: don't load non-essential kexts
# keepsyms=1     Keep symbol names in kernel panic backtraces
# debug=0x14e    Enable kernel debugger + wait on panic
# kextlog=0xfff  Maximum kext loading verbosity
# rd=md0         Use ramdisk (md0) as root device
# init=/sbin/init  Explicit path to our init binary
#

BOOT_ARGS="serial=2 -v -x keepsyms=1 debug=0x14e kextlog=0xfff"

if [[ -f "$RAMDISK" ]]; then
    BOOT_ARGS="$BOOT_ARGS rd=md0 init=/sbin/init"
fi

# ── QEMU flags ────────────────────────────────────────────────────────────────
QEMU_ARGS=(
    -M q35                          # modern chipset: PCIe, AHCI, IOAPIC
    -cpu Haswell                    # CPU model with good XNU compatibility
    -m 1024                         # 1 GiB RAM (XNU needs at least 512 MiB)
    -kernel "$KERNELCACHE"
    -append "$BOOT_ARGS"
    -serial stdio                   # serial port -> your terminal
    -no-reboot                      # stop on triple fault instead of rebooting
    -d guest_errors,unimp           # log guest errors to stderr
)

# Serial-only (no VGA window)
if [[ "$NOVGA" == "1" ]]; then
    QEMU_ARGS+=(-nographic)
fi

# Ramdisk as second drive
if [[ -f "$RAMDISK" ]]; then
    QEMU_ARGS+=(
        -drive "file=$RAMDISK,format=raw,if=none,id=ramdisk,readonly=on"
        -device "virtio-blk-pci,drive=ramdisk"
    )
fi

# Primary (blank) disk
QEMU_ARGS+=(
    -drive "file=$DISK_IMG,format=raw,if=none,id=hd0"
    -device "virtio-blk-pci,drive=hd0"
)

# GDB/LLDB remote debug server
if [[ "$DEBUG_MODE" == "1" ]]; then
    QEMU_ARGS+=(-s -S)   # -s = GDB server on :1234, -S = pause at first insn
    echo ""
    echo "  *** DEBUG MODE ***"
    echo "  QEMU paused. Connect with:"
    echo "    lldb build/out/kernel.development"
    echo "    (lldb) gdb-remote localhost:1234"
    echo "    (lldb) continue"
    echo ""
fi

# ── Launch ────────────────────────────────────────────────────────────────────
echo "==> Launching JULIUS OS in QEMU (x86_64)"
echo "    Kernel:    $KERNELCACHE"
[[ -f "$RAMDISK" ]] && echo "    Ramdisk:   $RAMDISK"
echo "    Boot args: $BOOT_ARGS"
echo ""
echo "  Press Ctrl+A then X to quit QEMU"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
