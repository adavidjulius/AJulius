# JULIUS OS — Development Roadmap

## Phase 0 — "Kernel alive" ✦ Current phase

**Goal:** XNU DEVELOPMENT kernel boots in QEMU with serial output.

**Deliverable:** `"JULIUS OS kernel alive"` from `kprintf()` on the serial console.

Tasks:
- [ ] Fix XNU build environment (AvailabilityVersions, dtrace CTF tools)
- [ ] Build XNU DEVELOPMENT kernel for x86_64
- [ ] Boot in QEMU with `-serial stdio`
- [ ] See `kprintf` output without any kexts (`-x -s` flags)
- [ ] Build XNU DEVELOPMENT kernel for ARM64
- [ ] Boot ARM64 in QEMU virt

---

## Phase 1 — "Hello from PID 1"

**Goal:** Custom init process runs as PID 1 and prints the JULIUS OS banner.

**Deliverable:** Banner + `"init: PID 1 is alive"` printed from user space.

Tasks:
- [ ] Build `julius-libc` (write, exit, vm_allocate stubs)
- [ ] Build `julius-init` (static Mach-O, no system libraries)
- [ ] Create minimal ramdisk with `/sbin/init`
- [ ] Pass `rd=md0 init=/sbin/init` boot args
- [ ] Verify with `file` and `otool -L` that binary is fully static
- [ ] Mach bootstrap port allocated and registered

---

## Phase 2 — "IPC works"

**Goal:** Two processes communicate via Mach IPC without Apple libraries.

**Deliverable:** Init forks a child; child sends a Mach message to init's
bootstrap port; init receives and prints the message.

Tasks:
- [ ] Implement `fork()` via BSD syscall in `julius-libc`
- [ ] Implement `execve()` stub
- [ ] Implement full `mach_msg()` send + receive
- [ ] Write a `julius-ping` test program
- [ ] Wire `wait4()` for zombie reaping in init

---

## Phase 3 — "Driver stack"

**Goal:** JuliusSerial kext loads and provides a registered I/O Kit service.

**Deliverable:** UART output routed through the I/O Kit stack; init looks up
the service via `IOServiceGetMatchingService`.

Tasks:
- [ ] Build `JuliusSerial.kext` and bundle it into the kernelcache
- [ ] Verify kext loads via `kextlog` output
- [ ] Expose a simple user-client interface
- [ ] Init opens the user-client and writes through it
- [ ] Add `IOInterruptEventSource` for RX interrupts (replaces polling)

---

## Phase 4 — "Filesystem and shell"

**Goal:** Read-write filesystem, process management, interactive shell.

**Deliverable:** A `$` prompt where you can run `ls`, `echo`, `cat`.

Tasks:
- [ ] Port or write a FAT32 kext (or use Darwin's existing HFS+ kext)
- [ ] Implement `open/read/write/close/stat` in `julius-libc`
- [ ] Port a minimal shell (consider: `dash`, or write a tiny one from scratch)
- [ ] Port `coreutils` basics statically linked against `julius-libc`
- [ ] Process group / session management in init
- [ ] `/dev/console` device node

---

## Phase 5 — "Custom silicon"

**Goal:** JULIUS OS boots on physical custom ARM64 hardware.

**Deliverable:** Banner on real hardware's serial port.

Tasks:
- [ ] Design custom chip memory map (UART base, DRAM region, interrupt controller)
- [ ] Write `pexpert/arm64/julius_pe.c` for the custom chip
- [ ] Create device tree blob for custom board
- [ ] Write or adapt a bootloader (U-Boot or custom) to load the kernelcache
- [ ] JTAG debugging setup for early-boot failures
- [ ] Port JuliusSerial to the custom UART IP

---

## Phase 6 — "Network and services"

**Goal:** TCP/IP networking, service discovery, remote shell.

Tasks:
- [ ] XNU BSD networking stack (already in XNU — need a NIC kext)
- [ ] Write a VirtIO-net kext for QEMU testing
- [ ] Implement a minimal DNS resolver
- [ ] Bring up `sshd` or a custom remote console daemon
- [ ] Mach bootstrap service registry (full `bootstrap_look_up` protocol)

---

## Long-term vision

- Custom GUI (Metal-like GPU driver, compositor)
- Security model (mandatory access control, custom entitlements)
- Swift/C++ runtime support on julius-libc
- Replace Mach IPC with a custom higher-performance IPC protocol
- JULIUS OS running JULIUS OS (self-hosting compiler)

---

## Version control strategy

```
main            ← always boots (even if features are minimal)
dev/phase-N     ← active development branch for phase N
feat/xyz        ← individual feature branches
```

Tag each phase completion:
```bash
git tag -a v0.1.0 -m "Phase 0: kernel boots in QEMU"
git tag -a v0.2.0 -m "Phase 1: PID 1 prints banner"
```
