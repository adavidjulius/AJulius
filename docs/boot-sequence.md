# JULIUS OS Boot Sequence

This document traces the complete boot sequence from power-on to the JULIUS OS
banner appearing on the serial console.

---

## Overview

```
Power on
  └─ UEFI / QEMU firmware
       └─ Loads kernelcache (Mach-O 64-bit)
            └─ XNU _start  (osfmk/x86_64/start.s or osfmk/arm64/start.s)
                 └─ PE_init_platform()   ← pexpert: early serial output
                      └─ machine_startup()
                           └─ VM + scheduler init
                                └─ BSD init
                                     └─ exec("/sbin/init")
                                          └─ julius_start()   ← our PID 1
                                               └─ banner + IPC loop
```

---

## Phase 0: Firmware

### x86_64 (QEMU q35)

QEMU emulates a UEFI firmware (SeaBIOS or OVMF). It:

1. Runs POST and enumerates virtual devices (PCIe, AHCI, VirtIO).
2. Looks for a bootable partition or a `-kernel` argument (what we use).
3. When `-kernel julius.kernelcache` is passed, QEMU loads the Mach-O
   kernelcache into memory at the correct load addresses (read from the
   Mach-O segment load commands).
4. Builds a synthetic EFI environment and jumps to the kernel entry point.

### ARM64 (QEMU virt)

QEMU's virt machine uses a minimal firmware blob. It:

1. Places the kernelcache at `0x40000000` in physical memory.
2. Sets `x0` = physical address of the `boot_args` struct.
3. Jumps to the first instruction of the kernel (`_start` in `osfmk/arm64/start.s`).

---

## Phase 1: Kernel Entry

### x86_64 — `osfmk/x86_64/start.s`

```asm
_start:
    /* Set up GDT, IDT */
    /* Enable 64-bit paging */
    /* Set up the initial stack */
    call i386_init
```

`i386_init()` calls:
- `PE_init_platform(FALSE, &args)` — **Phase 1 pexpert** (early UART, no VM yet)
- `machine_startup()` — sets up paging, interrupts, SMP

### ARM64 — `osfmk/arm64/start.s`

```asm
_start:
    /* MMU disabled — running physical addresses */
    /* Set up exception vectors */
    /* Enable MMU with identity map */
    b arm_init
```

`arm_init()` calls:
- `PE_init_platform(FALSE, &args)` — **Phase 1 pexpert**

---

## Phase 2: Platform Expert (Early)

`PE_init_platform(vm_initialized=FALSE, ...)` is the **first JULIUS-controlled
code that runs**. At this point:

- Virtual memory is **not set up** yet.
- We must use physical addresses only.
- We can write to the UART directly to prove life.

Our `pexpert/arm64/julius_pe.c`:

```c
void PE_init_platform(boolean_t vm_initialized, void *args) {
    if (!vm_initialized) {
        /* Phase 1: Direct physical UART access */
        uart_base = (volatile uint32_t *)0x09000000;
        uart_putc('J');
        return;
    }
    /* Phase 2: VM is up, remap UART properly */
    uart_base = (volatile uint32_t *)ml_io_map(0x09000000, 0x1000);
}
```

---

## Phase 3: VM, Scheduler, BSD Init

XNU's standard bootstrap sequence (not modified by JULIUS OS at this stage):

1. `vm_mem_init()` — set up the virtual memory subsystem.
2. `PE_init_platform(TRUE, ...)` — **Phase 2 pexpert**, UART remapped.
3. `ipc_init()` — Mach IPC tables.
4. `sched_init()` — thread scheduler.
5. `bsd_init()` — BSD UNIX layer (file descriptors, process table, VFS).
6. `IOKitBSDInit()` — start the I/O Kit matching engine.
   - This is where **JuliusSerial.kext** gets matched and started.
7. `bsd_utaskbootstrap()` — creates the first user-space task and execs PID 1.

---

## Phase 4: PID 1 (julius-init)

The kernel execs `/sbin/init` (from the ramdisk) as PID 1.

XNU's exec ABI places the following on the stack at entry:
```
[rsp+0]   argc = 0
[rsp+8]   argv[0] = pointer to program name
...       envp, apple[] strings
```

Our `start.s` entry stub:
1. Zeros `rbp` (marks outermost frame for debuggers).
2. Aligns stack to 16 bytes.
3. Calls `julius_start(argc, argv)`.

`julius_start()` in `main.c`:
1. Calls `julius_puts()` → writes banner to fd 2 (serial console).
2. Calls `julius_mem_init()` → allocates 1 MiB heap via `mach_vm_allocate`.
3. Calls `setup_bootstrap_port()` → allocates a Mach receive port, inserts
   send right, sets it as the task's bootstrap port.
4. Enters `service_loop()` — blocks forever on `mach_msg()`.

**PID 1 must never exit.** If it does, XNU panics:
```
panic: launchd died
```

---

## Boot arguments reference

Boot arguments are passed via the `-append` flag in QEMU (x86) or via the
`boot_args` struct (ARM64).

| Argument | Effect |
|---|---|
| `serial=2` | Route `kprintf` to COM1 serial (x86) |
| `serial=3` | Route `kprintf` to PL011 UART (ARM) |
| `earlycon` | Enable console before drivers load (ARM) |
| `-v` | Verbose: all `kprintf` output visible |
| `-x` | Safe mode: skip non-essential kexts |
| `-s` | Single-user mode |
| `keepsyms=1` | Symbol names in panic backtraces |
| `debug=0x14e` | Kernel debugger + wait on panic |
| `kextlog=0xfff` | Verbose kext loading log |
| `rd=md0` | Use ramdisk (md0) as root device |
| `init=/sbin/init` | Explicit path to PID 1 binary |
| `nvram-boot-args` | Override NVRAM boot args |

---

## Debugging a boot failure

### Symptom: nothing on serial

- Is `serial=2` (x86) or `serial=3` (ARM) in boot args?
- Is QEMU's `-serial stdio` connected?
- Does the kernelcache Mach-O have correct load addresses? Run `otool -l julius.kernelcache`.

### Symptom: early panic before VM is set up

- The kernel panics before `kprintf` works — output is garbled or missing.
- Add `PE_early_puts("alive\n", 6)` calls directly in the XNU source.
- Use QEMU `-s -S` + LLDB to break at `_start`.

### Symptom: `launchd died` panic

- PID 1 exited. Check that `julius_start()` is NORETURN and that
  `service_loop()` actually loops forever.
- Check the ramdisk: does `/sbin/init` exist and is it executable?

### Symptom: `No init process found`

- Kernel cannot exec `/sbin/init`.
- Is the ramdisk properly formatted (HFS+)?
- Is the `init=` boot argument correct?
- Is the binary a valid static Mach-O for the right arch?
  ```bash
  file build/out/julius-init_x86_64
  # should say: Mach-O 64-bit executable x86_64
  ```
