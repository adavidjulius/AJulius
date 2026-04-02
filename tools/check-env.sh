#!/usr/bin/env bash
# tools/check-env.sh
# JULIUS OS — Development environment health check
#
# Run this before your first build to catch missing dependencies early.
# Usage: ./tools/check-env.sh

set -euo pipefail

PASS=0
FAIL=0
WARN=0

pass() { echo "  ✓ $1"; PASS=$((PASS+1)); }
fail() { echo "  ✗ $1"; FAIL=$((FAIL+1)); }
warn() { echo "  ! $1"; WARN=$((WARN+1)); }

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  JULIUS OS — Environment Check"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# ── macOS version ──────────────────────────────────────────────────────────
echo "[ System ]"
MACOS_VER="$(sw_vers -productVersion 2>/dev/null || echo unknown)"
case "$MACOS_VER" in
    13.*|14.*|15.*|16.*) pass "macOS $MACOS_VER (supported)" ;;
    12.*)                warn "macOS $MACOS_VER (older, may work)" ;;
    *)                   fail "macOS $MACOS_VER (unsupported)" ;;
esac

ARCH_NATIVE="$(uname -m)"
pass "Architecture: $ARCH_NATIVE"

# ── Xcode ──────────────────────────────────────────────────────────────────
echo ""
echo "[ Xcode / SDK ]"
if xcode-select -p &>/dev/null; then
    XCODE_PATH="$(xcode-select -p)"
    pass "Xcode path: $XCODE_PATH"
else
    fail "Xcode command-line tools not installed. Run: xcode-select --install"
fi

if xcrun --sdk macosx --show-sdk-path &>/dev/null; then
    SDK="$(xcrun --sdk macosx --show-sdk-path)"
    SDK_VER="$(xcrun --sdk macosx --show-sdk-version 2>/dev/null || echo unknown)"
    pass "macOS SDK $SDK_VER: $SDK"
else
    fail "macOS SDK not found"
fi

# Check for Kernel.framework (needed for kext builds)
KEXT_HEADERS="$SDK/System/Library/Frameworks/Kernel.framework/Headers"
if [[ -d "$KEXT_HEADERS" ]]; then
    pass "Kernel.framework headers found"
else
    warn "Kernel.framework headers missing (kext builds will fail)"
fi

# ── Compilers ──────────────────────────────────────────────────────────────
echo ""
echo "[ Compilers ]"
check_tool() {
    local name="$1"; shift
    if command -v "$name" &>/dev/null; then
        local ver
        ver="$("$name" "$@" 2>&1 | head -1)"
        pass "$name: $ver"
    else
        fail "$name: not found"
    fi
}

check_tool clang --version
check_tool clang++ --version
check_tool ld -v

# Check clang target support
if clang -target x86_64-apple-macos13 -x c /dev/null -o /dev/null 2>/dev/null; then
    pass "clang: x86_64-apple-macos13 target works"
else
    fail "clang: x86_64-apple-macos13 target failed"
fi

# ── Build tools ────────────────────────────────────────────────────────────
echo ""
echo "[ Build tools ]"
for tool in make ar nm otool file; do
    if command -v "$tool" &>/dev/null; then
        pass "$tool: $(command -v "$tool")"
    else
        fail "$tool: not found"
    fi
done

# ── Homebrew tools ─────────────────────────────────────────────────────────
echo ""
echo "[ Optional tools (brew install) ]"
for tool in qemu cmake ninja python3; do
    if command -v "$tool" &>/dev/null; then
        ver="$("$tool" --version 2>&1 | head -1)"
        pass "$tool: $ver"
    else
        warn "$tool: not found  (brew install $tool)"
    fi
done

# QEMU target check
if command -v qemu-system-x86_64 &>/dev/null; then
    pass "qemu-system-x86_64 available"
else
    warn "qemu-system-x86_64 not found  (brew install qemu)"
fi
if command -v qemu-system-aarch64 &>/dev/null; then
    pass "qemu-system-aarch64 available"
else
    warn "qemu-system-aarch64 not found  (brew install qemu)"
fi

# ── Git submodules ─────────────────────────────────────────────────────────
echo ""
echo "[ Git submodules ]"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "$REPO_ROOT/.gitmodules" ]]; then
    pass ".gitmodules present"
    if [[ -d "$REPO_ROOT/kernel/xnu/osfmk" ]]; then
        pass "kernel/xnu submodule initialised"
    else
        warn "kernel/xnu not initialised. Run: git submodule update --init --recursive"
    fi
    if [[ -d "$REPO_ROOT/kernel/dtrace" ]]; then
        pass "kernel/dtrace submodule initialised"
    else
        warn "kernel/dtrace not initialised"
    fi
else
    warn ".gitmodules not found"
fi

# ── Python ─────────────────────────────────────────────────────────────────
echo ""
echo "[ Python (for tools) ]"
if python3 -c "import sys; assert sys.version_info >= (3, 8)" 2>/dev/null; then
    pass "Python 3.8+: $(python3 --version)"
else
    warn "Python 3.8+ not found"
fi

# ── Summary ────────────────────────────────────────────────────────────────
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Results: ${PASS} passed  ${WARN} warnings  ${FAIL} failed"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if [[ "$FAIL" -gt 0 ]]; then
    echo "  Some required tools are missing. Fix the failures before building."
    exit 1
elif [[ "$WARN" -gt 0 ]]; then
    echo "  Environment is usable. Install optional tools for full functionality."
    exit 0
else
    echo "  Environment is ready. Run: ./build/build-all.sh --skip-kernel"
    exit 0
fi
