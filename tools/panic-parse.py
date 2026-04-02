#!/usr/bin/env python3
"""
tools/panic-parse.py
JULIUS OS — Kernel Panic Log Parser

Parses XNU kernel panic output from the QEMU serial console and
symbolizes raw addresses using the kernel dSYM / symbol file.

Usage:
    # Pipe QEMU serial output directly:
    ./build/run-qemu-x86.sh 2>&1 | python3 tools/panic-parse.py

    # Parse a saved log:
    python3 tools/panic-parse.py panic.log

    # With explicit symbol file:
    python3 tools/panic-parse.py panic.log --symbols build/out/kernel.development

The script:
  1. Detects the "panic(...)" line and the backtrace section.
  2. Extracts hex addresses from the backtrace.
  3. Runs atos / llvm-symbolizer to convert them to symbol+offset.
  4. Prints a coloured, annotated report.
"""

import sys
import re
import subprocess
import os
from pathlib import Path

# ── ANSI colours ──────────────────────────────────────────────────────────────
RED    = "\033[91m"
YELLOW = "\033[93m"
GREEN  = "\033[92m"
CYAN   = "\033[96m"
BOLD   = "\033[1m"
RESET  = "\033[0m"

def colour(text, code):
    if sys.stdout.isatty():
        return f"{code}{text}{RESET}"
    return text

# ── Panic log patterns ────────────────────────────────────────────────────────
PANIC_LINE_RE   = re.compile(r'panic\(cpu \d+ caller (0x[0-9a-fA-F]+)\)', re.IGNORECASE)
BACKTRACE_RE    = re.compile(r'(0x[0-9a-fA-F]{8,16})\s*:\s*(.+?)(?:\+0x[0-9a-fA-F]+)?$')
ADDRESS_RE      = re.compile(r'0x[0-9a-fA-F]{8,16}')
KEXT_RE         = re.compile(r'(com\.[a-zA-Z0-9._-]+)\s+@\s+(0x[0-9a-fA-F]+)')

# ── Known panic codes ─────────────────────────────────────────────────────────
KNOWN_PANICS = {
    "page fault in kernel mode":   "Null pointer or unmapped memory dereference.",
    "Unresolved kernel trap":       "A kext called a symbol not present in XNU. Check OSBundleLibraries versions.",
    "launchd died":                 "PID 1 exited. julius-init must never return from julius_start().",
    "No init process found":        "Kernel could not exec /sbin/init. Check ramdisk and init= boot arg.",
    "Root device not found":        "Kernel cannot mount root filesystem. Use rd=md0 with a ramdisk.",
    "Kernel trap at":               "Hardware exception in kernel mode (GPF, invalid opcode, etc.).",
    "spinlock timeout":             "A spinlock was held too long. Likely infinite loop or deadlock in driver.",
    "stack overflow":               "Kernel stack overflowed. Reduce recursion depth or increase stack size.",
    "zone map exhausted":           "Kernel memory allocator ran out of zone memory. Possible memory leak.",
    "assertion failed":             "A kernel assertion (assert() / KERNEL_DEBUG_CONSTANT) triggered.",
}

def find_known_panic(text):
    for key, explanation in KNOWN_PANICS.items():
        if key.lower() in text.lower():
            return explanation
    return None

# ── Symbolizer ────────────────────────────────────────────────────────────────
def symbolize(address_hex, symbol_file):
    """Convert a hex address to symbol+offset using atos or llvm-symbolizer."""
    addr = int(address_hex, 16)

    # Try macOS atos first (best for Mach-O / dSYM)
    try:
        result = subprocess.run(
            ["atos", "-o", symbol_file, "-arch", "x86_64", hex(addr)],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0 and result.stdout.strip():
            sym = result.stdout.strip()
            if sym != hex(addr):    # atos returns address unchanged if unknown
                return sym
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    # Try llvm-symbolizer
    try:
        result = subprocess.run(
            ["llvm-symbolizer", "--obj", symbol_file, hex(addr)],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip().split("\n")[0]
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    return None   # could not symbolize

# ── Main parser ───────────────────────────────────────────────────────────────
def parse_panic_log(lines, symbol_file=None):
    in_backtrace = False
    panic_summary = []
    backtrace_frames = []
    kext_list = []
    raw_panic_text = []

    for line in lines:
        line = line.rstrip()

        # Detect start of panic
        if "panic(" in line.lower() or "kernel panic" in line.lower():
            raw_panic_text.append(line)
            in_backtrace = False

        # Collect panic description lines (before backtrace)
        if raw_panic_text and not in_backtrace:
            raw_panic_text.append(line)
            explanation = find_known_panic(line)
            if explanation:
                panic_summary.append(explanation)

        # Detect backtrace section
        if "Backtrace" in line or "backtrace" in line:
            in_backtrace = True
            continue

        # Parse backtrace frames
        if in_backtrace:
            m = BACKTRACE_RE.match(line.strip())
            if m:
                addr = m.group(1)
                sym  = m.group(2).strip()
                backtrace_frames.append((addr, sym))
            # Detect kext list
            km = KEXT_RE.search(line)
            if km:
                kext_list.append((km.group(1), km.group(2)))
            # End of backtrace
            if line.strip() == "" and backtrace_frames:
                in_backtrace = False

    # ── Print report ─────────────────────────────────────────────────────────
    print()
    print(colour("═" * 60, BOLD))
    print(colour("  JULIUS OS — KERNEL PANIC REPORT", BOLD + RED))
    print(colour("═" * 60, BOLD))
    print()

    if raw_panic_text:
        print(colour("Panic message:", BOLD))
        for l in raw_panic_text[:5]:
            print(f"  {colour(l, RED)}")
        print()

    if panic_summary:
        print(colour("Diagnosis:", BOLD))
        for s in panic_summary:
            print(f"  {colour('→', YELLOW)} {s}")
        print()

    if backtrace_frames:
        print(colour(f"Backtrace ({len(backtrace_frames)} frames):", BOLD))
        for i, (addr, sym) in enumerate(backtrace_frames):
            sym_str = sym

            # Try to symbolize if we have a symbol file
            if symbol_file and os.path.exists(symbol_file):
                resolved = symbolize(addr, symbol_file)
                if resolved:
                    sym_str = colour(resolved, GREEN)

            frame_colour = CYAN if i == 0 else RESET
            print(f"  #{i:<3} {colour(addr, frame_colour)}  {sym_str}")
        print()

    if kext_list:
        print(colour("Loaded kexts:", BOLD))
        for name, base in kext_list:
            tag = colour("JULIUS", GREEN) if "julius" in name.lower() else ""
            print(f"  {base}  {name}  {tag}")
        print()

    print(colour("Common fixes:", BOLD))
    print(f"  {colour('•', YELLOW)} Add 'keepsyms=1' to boot args for symbol names in panics")
    print(f"  {colour('•', YELLOW)} Run with --debug to attach LLDB: lldb build/out/kernel.development")
    print(f"  {colour('•', YELLOW)} Check kprintf output above the panic for the last known-good state")
    print(f"  {colour('•', YELLOW)} If PID 1 died: ensure julius_start() never returns")
    print()

# ── Entry point ───────────────────────────────────────────────────────────────
def main():
    import argparse

    p = argparse.ArgumentParser(description="JULIUS OS panic log parser")
    p.add_argument("logfile", nargs="?", help="Panic log file (or stdin)")
    p.add_argument("--symbols", "-s", default=None,
                   help="Path to kernel symbol file / dSYM for address resolution")
    args = p.parse_args()

    # Auto-detect symbol file
    symbol_file = args.symbols
    if not symbol_file:
        candidates = [
            "build/out/kernel.development",
            "/tmp/julius-xnu-sym/System/Library/Kernels/kernel.development",
        ]
        for c in candidates:
            if os.path.exists(c):
                symbol_file = c
                print(f"[panic-parse] Using symbols: {symbol_file}", file=sys.stderr)
                break

    if args.logfile and args.logfile != "-":
        with open(args.logfile) as f:
            lines = f.readlines()
    else:
        print("[panic-parse] Reading from stdin (pipe QEMU output here)...",
              file=sys.stderr)
        lines = sys.stdin.readlines()

    parse_panic_log(lines, symbol_file)

if __name__ == "__main__":
    main()
