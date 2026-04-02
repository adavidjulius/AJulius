# JULIUS OS

JULIUS OS is an advanced-purpose operating system built on the XNU kernel, with a fully custom user-space. No Apple libraries (no `libSystem`, no `launchd`) — everything from PID 1 upward is written from scratch.

---

## Goals

- Replace all Apple user-space components with custom implementations
- Build a minimal, auditable, controllable system from the ground up
- Implement a custom `libc` that invokes XNU syscalls directly
- Build a custom `init` (PID 1) that bootstraps Mach IPC services
- Support future custom hardware platforms (custom ARM64 chip)

---

## Architecture

```
┌─────────────────────────────────────────┐
│         User Applications               │  ← your future shell, services
├───────────────┬─────────────────────────┤
│  julius-init  │      julius-libc        │  ← PID 1 + minimal C library
├───────────────┴─────────────────────────┤
│     Mach IPC + BSD Syscall Interface    │  ← raw syscall ABI
├──────────────┬──────────┬───────────────┤
│ Mach kernel  │  BSD     │   I/O Kit     │  ← XNU internals
├──────────────┴──────────┴───────────────┤
│         pexpert / Platform Expert       │  ← hardware abstraction
├─────────────────────┬───────────────────┤
│   UEFI / iBoot      │   QEMU (dev)      │  ← bootloader
├─────────────────────┴───────────────────┤
│   Hardware: x86_64 → custom ARM64       │
└─────────────────────────────────────────┘
```

| Layer | Component | Status |
|---|---|---|
| Kernel | XNU (modified) | 🚧 Building |
| libc | julius-libc | 🚧 In progress |
| Init | julius-init (PID 1) | 🚧 In progress |
| Serial driver | JuliusSerial (I/O Kit kext) | 🚧 In progress |
| Boot | QEMU x86_64 target | 🚧 Setting up |

---

## Project Structure

```
AJulius/
├── kernel/          # XNU source (git submodule) + JULIUS patches
├── libc/
│   ├── include/     # Public headers (julius/syscall.h, julius/types.h)
│   └── src/         # syscall stubs, write, malloc, string helpers
├── init/
│   └── src/         # julius-init: PID 1, Mach bootstrap, IPC loop
├── kexts/
│   └── JuliusSerial/ # I/O Kit UART driver (PL011 / 16550)
├── build/           # Build scripts (kernel, libc, init, kexts, QEMU)
├── tools/           # Debugging utilities, panic log parser
├── docs/            # Design notes, syscall ABI, boot sequence
└── README.md
```

---

## Quick Start (Development on macOS)

### 1. Prerequisites

```bash
# Xcode + command-line tools
xcode-select --install
sudo xcodebuild -license accept

# Build tools
brew install cmake ninja python3 llvm qemu

# Cross-compilers (for ARM64 cross-compilation)
brew install aarch64-elf-gcc x86_64-elf-gcc
```

### 2. Clone and set up XNU submodule

```bash
git clone https://github.com/adavidjulius/AJulius.git
cd AJulius
git submodule update --init --recursive   # pulls XNU into kernel/xnu/
```

### 3. Build the libc and init

```bash
cd build
./build-libc.sh
./build-init.sh
```

### 4. Build the kernel

```bash
./build-kernel.sh
```

### 5. Run in QEMU

```bash
./run-qemu-x86.sh
```

Expected output on the serial console:

```
*** JULIUS OS ***
init: PID 1 is alive
init: setting up Mach bootstrap port...
init: bootstrap port established
```

---

## Syscall ABI

XNU on x86_64 uses two syscall classes:

| Class | Selector | Example |
|---|---|---|
| BSD | `0x2000000 \| n` | `write` = `0x2000004` |
| Mach trap | negative number | `mach_task_self` = `-28` |

Calling convention: syscall number in `rax`, args in `rdi, rsi, rdx, r10, r8, r9`, invoke with `syscall`.

On ARM64: syscall number in `x16`, args in `x0–x5`, invoke with `svc #0x80`.

See `docs/syscall-abi.md` for the full table.

---

## Current Status

🚧 **Bootstrapping phase** — build system, minimal libc, and PID 1 init.

Follow progress in [Issues](https://github.com/adavidjulius/AJulius/issues) and [Projects](https://github.com/adavidjulius/AJulius/projects).

---

## Contributing

This is a solo research project for now. Architecture discussions welcome via Issues.

## License

BSD 2-Clause (matching XNU's open-source components). See `LICENSE`.
