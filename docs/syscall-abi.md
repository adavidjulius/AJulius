# JULIUS OS — XNU Syscall ABI Reference

This document describes the exact calling conventions and syscall numbers
used by `julius-libc` to invoke XNU kernel services without any Apple
libraries.

---

## x86_64 Calling Convention

XNU on x86_64 uses the `syscall` instruction (not `int 0x80`).

| Register | Role |
|---|---|
| `rax` | Syscall number (input) / return value (output) |
| `rdi` | Argument 1 |
| `rsi` | Argument 2 |
| `rdx` | Argument 3 |
| `r10` | Argument 4 (NOT rcx — the syscall instruction clobbers rcx) |
| `r8`  | Argument 5 |
| `r9`  | Argument 6 |
| `rcx`, `r11` | Clobbered by the syscall instruction |

Return value is in `rax`. On error, `rax` contains a negative errno value
(BSD) or a non-zero `kern_return_t` (Mach).

---

## ARM64 Calling Convention

| Register | Role |
|---|---|
| `x16` | Syscall number |
| `x0`  | Argument 1 / return value |
| `x1`  | Argument 2 |
| `x2`  | Argument 3 |
| `x3`  | Argument 4 |
| `x4`  | Argument 5 |
| `x5`  | Argument 6 |

Invoke with: `svc #0x80`

---

## Syscall Number Classes

### BSD Syscalls (positive numbers)

The BSD syscall selector adds `0x2000000` to the number:

```c
#define BSD_SYSCALL(n)  (0x2000000UL | (n))
```

So `write` (number 4) = `0x2000004` in `rax`.

The full BSD table is in `bsd/kern/syscalls.master` in the XNU source.

### Mach Traps (negative numbers)

Mach traps use negative numbers. The kernel negates the value to index
into its trap table.

```c
#define MACH_TRAP(n)  (-(n))
// e.g. mach_task_self (slot 28) = -28
```

The Mach trap table is in `osfmk/kern/syscall_sw.c`.

---

## BSD Syscall Table (used by JULIUS OS)

| Name | Number | rax value | Signature |
|---|---|---|---|
| `exit` | 1 | `0x2000001` | `(int status)` → never returns |
| `fork` | 2 | `0x2000002` | `(void)` → pid_t |
| `read` | 3 | `0x2000003` | `(int fd, void *buf, size_t count)` → ssize_t |
| `write` | 4 | `0x2000004` | `(int fd, const void *buf, size_t count)` → ssize_t |
| `open` | 5 | `0x2000005` | `(const char *path, int flags, mode_t mode)` → int |
| `close` | 6 | `0x2000006` | `(int fd)` → int |
| `wait4` | 7 | `0x2000007` | `(pid_t pid, int *status, int options, ...)` → pid_t |
| `getpid` | 20 | `0x2000014` | `(void)` → pid_t |
| `kill` | 37 | `0x2000025` | `(pid_t pid, int sig)` → int |
| `execve` | 59 | `0x200003B` | `(const char *path, char **argv, char **envp)` → int |
| `mach_port_insert_right` | 0x218 | `0x2000218` | `(task, name, poly, poly_type)` → kern_return_t |

---

## Mach Trap Table (used by JULIUS OS)

| Name | Slot | rax value | Signature |
|---|---|---|---|
| `host_self_trap` | 20 | `-20` | `(void)` → mach_port_t |
| `mach_reply_port` | 26 | `-26` | `(void)` → mach_port_t |
| `thread_self_trap` | 27 | `-27` | `(void)` → mach_port_t |
| `task_self_trap` | 28 | `-28` | `(void)` → mach_port_t (= mach_task_self()) |
| `mach_msg_trap` | 31 | `-31` | `(msg, option, send_sz, rcv_sz, rcv_port, timeout, notify)` |
| `mach_msg2_trap` | 32 | `-32` | newer mach_msg variant |
| `mach_wait_until` | 59 | `-59` | `(uint64_t deadline)` → kern_return_t |
| `mach_port_allocate` | 16 | `-16` | `(task, right, *name)` → kern_return_t |
| `mach_vm_allocate` | 10 | `-10` | `(task, *addr, size, flags)` → kern_return_t |
| `mach_vm_deallocate` | 12 | `-12` | `(task, addr, size)` → kern_return_t |
| `task_set_bootstrap_port` | 88 | `-88` | `(task, bootstrap_port)` → kern_return_t |

---

## Worked examples

### `write("hello\n")` to fd 2 (serial console)

```asm
; rax = 0x2000004 (SYS_write)
; rdi = 2        (fd = stderr)
; rsi = <buf>    (pointer to "hello\n")
; rdx = 6        (length)
mov  $0x2000004, %rax
mov  $2,         %rdi
lea  hello(%rip),%rsi
mov  $6,         %rdx
syscall
; rax = bytes written (6) on success, negative errno on error
```

### `mach_task_self()` — get own task port

```asm
mov  $-28, %rax    ; TRAP_task_self_trap
syscall
; rax = mach_port_t for the calling task
```

### `mach_vm_allocate` — allocate 4096 bytes anywhere

```c
mach_vm_address_t addr = 0;
// rax = -10, rdi = task_self, rsi = &addr, rdx = 4096, r10 = VM_FLAGS_ANYWHERE(1)
kern_return_t kr = julius_vm_allocate(julius_mach_task_self(),
                                       &addr, 4096, VM_FLAGS_ANYWHERE);
// addr now holds the kernel-chosen virtual address
```

---

## Error handling

BSD syscalls return a negative errno on error (e.g. `-9` = EBADF).

Mach traps return a `kern_return_t`:

| Value | Meaning |
|---|---|
| `0` | `KERN_SUCCESS` |
| `4` | `KERN_INVALID_ARGUMENT` |
| `5` | `KERN_FAILURE` |
| `10` | `KERN_NO_SPACE` |
| `11` | `KERN_INVALID_ADDRESS` |
| `28` | `KERN_NO_ACCESS` |

The full list is in `osfmk/mach/kern_return.h`.
