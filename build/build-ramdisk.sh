#!/usr/bin/env bash
# build/build-ramdisk.sh
# JULIUS OS — Build a minimal ramdisk image containing julius-init
#
# The ramdisk is a flat HFS+ or APFS image that XNU mounts as the root
# filesystem when passed the rd=md0 boot argument.
#
# Layout inside the ramdisk:
#   /sbin/init     -> julius-init binary (PID 1)
#   /dev/          -> empty, kernel populates it
#   /tmp/          -> empty
#
# Usage: ./build/build-ramdisk.sh [--arch x86_64|arm64]
# Output: build/out/julius-ramdisk.dmg

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/build/out"

ARCH="${1:-x86_64}"
case "$ARCH" in
    --arch) ARCH="$2" ;;
    x86_64|arm64) ;;
    *) echo "Usage: $0 [--arch x86_64|arm64]"; exit 1 ;;
esac

INIT_BIN="$OUT_DIR/julius-init_${ARCH}"
if [[ ! -f "$INIT_BIN" ]]; then
    echo "==> julius-init not found. Building..."
    "$REPO_ROOT/build/build-init.sh" --arch "$ARCH"
fi

RAMDISK="$OUT_DIR/julius-ramdisk.dmg"
MOUNT_POINT="/tmp/julius-ramdisk-mount"
DISK_SIZE="16m"   # 16 MiB — plenty for a minimal init

echo "==> Creating ramdisk: $RAMDISK ($DISK_SIZE)"

# Remove stale image if present
[[ -f "$RAMDISK" ]] && rm "$RAMDISK"

# Create a blank HFS+ disk image
hdiutil create \
    -size "$DISK_SIZE" \
    -fs HFS+ \
    -volname "JULIUS_ROOT" \
    -layout NONE \
    -type UDIF \
    "$RAMDISK"

# Mount it
mkdir -p "$MOUNT_POINT"
hdiutil attach "$RAMDISK" -mountpoint "$MOUNT_POINT" -nobrowse -quiet

echo "  Mounted at $MOUNT_POINT"

# Create the directory skeleton
mkdir -p "$MOUNT_POINT/sbin"
mkdir -p "$MOUNT_POINT/dev"
mkdir -p "$MOUNT_POINT/tmp"
mkdir -p "$MOUNT_POINT/var"
mkdir -p "$MOUNT_POINT/etc"

# Install init binary
echo "  Installing julius-init -> /sbin/init"
cp "$INIT_BIN" "$MOUNT_POINT/sbin/init"
chmod 755 "$MOUNT_POINT/sbin/init"

# Create a minimal /etc/rc (some XNU boot paths look for this)
cat > "$MOUNT_POINT/etc/rc" << 'EOF'
#!/bin/sh
# JULIUS OS /etc/rc — minimal
echo "JULIUS OS: /etc/rc executed" > /dev/console
EOF
chmod 755 "$MOUNT_POINT/etc/rc"

echo "  Ramdisk contents:"
find "$MOUNT_POINT" -not -path "$MOUNT_POINT" | sort | sed 's|'"$MOUNT_POINT"'||' | sed 's/^/    /'

# Unmount
hdiutil detach "$MOUNT_POINT" -quiet

echo ""
echo "==> Ramdisk built: $RAMDISK"
ls -lh "$RAMDISK"
echo ""
echo "  Use with: -append \"rd=md0 init=/sbin/init\""
