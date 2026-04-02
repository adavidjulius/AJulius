/*
 * julius/syscall.h
 * JULIUS OS — XNU Syscall ABI Definitions
 *
 * XNU exposes two classes of system calls to user space:
 *
 *   BSD syscalls:  number in rax = 0x2000000 | N
 *                  args in: rdi, rsi, rdx, r10, r8, r9
 *                  invoke:  syscall instruction
 *
 *   Mach traps:    number in rax = negative (e.g. -28 for mach_task_self)
 *                  same arg registers and syscall instruction
 *
 * On ARM64:
 *   syscall number in x16, args in x0..x5, invoke: svc #0x80
 *
 * References:
 *   bsd/kern/syscalls.master    (BSD numbers)
 *   osfmk/kern/syscall_sw.c    (Mach trap table)
 */

#pragma once
#include <julius/types.h>

/* ── BSD syscall numbers ────────────────────────────────────────────────── */

#define BSD_SYSCALL(n)     (0x2000000UL | (n))

#define SYS_exit           BSD_SYSCALL(1)
#define SYS_fork           BSD_SYSCALL(2)
#define SYS_read           BSD_SYSCALL(3)
#define SYS_write          BSD_SYSCALL(4)
#define SYS_open           BSD_SYSCALL(5)
#define SYS_close          BSD_SYSCALL(6)
#define SYS_wait4          BSD_SYSCALL(7)
#define SYS_getpid         BSD_SYSCALL(20)
#define SYS_getuid         BSD_SYSCALL(24)
#define SYS_kill           BSD_SYSCALL(37)
#define SYS_execve         BSD_SYSCALL(59)
#define SYS_munmap         BSD_SYSCALL(73)
#define SYS_mmap           BSD_SYSCALL(197)

/* ── Mach trap numbers ──────────────────────────────────────────────────── */
/* These are negative. The kernel negates them to index the trap table.     */

#define MACH_TRAP(n)       (-(n))

#define TRAP_mach_reply_port            MACH_TRAP(26)
#define TRAP_thread_self_trap           MACH_TRAP(27)
#define TRAP_task_self_trap             MACH_TRAP(28)   /* mach_task_self() */
#define TRAP_host_self_trap             MACH_TRAP(20)   /* mach_host_self() */
#define TRAP_mach_msg_trap              MACH_TRAP(31)
#define TRAP_mach_msg2_trap             MACH_TRAP(32)
#define TRAP_semaphore_signal_trap      MACH_TRAP(33)
#define TRAP_semaphore_wait_trap        MACH_TRAP(34)
#define TRAP_mach_wait_until            MACH_TRAP(59)
#define TRAP_mk_timer_create            MACH_TRAP(60)
#define TRAP_mach_port_allocate         MACH_TRAP(16)

/* ── Raw syscall wrappers (x86_64 only for now) ─────────────────────────── */

/*
 * julius_syscall0..6 are variadic-argument inline helpers that issue the
 * syscall instruction directly. Using "=a" output lets the compiler know
 * the return value lives in rax after the call.
 *
 * We use r10 for the 4th argument because the syscall instruction clobbers
 * rcx (Linux convention) — on XNU x86_64 the ABI uses r10 as arg4,
 * not rcx.
 */

static inline long julius_syscall0(long nr) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr)
        : "memory", "rcx", "r11"
    );
    return ret;
}

static inline long julius_syscall1(long nr, long a1) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr), "D"(a1)
        : "memory", "rcx", "r11"
    );
    return ret;
}

static inline long julius_syscall2(long nr, long a1, long a2) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr), "D"(a1), "S"(a2)
        : "memory", "rcx", "r11"
    );
    return ret;
}

static inline long julius_syscall3(long nr, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr), "D"(a1), "S"(a2), "d"(a3)
        : "memory", "rcx", "r11"
    );
    return ret;
}

static inline long julius_syscall4(long nr, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "memory", "rcx", "r11"
    );
    return ret;
}

static inline long julius_syscall6(long nr, long a1, long a2, long a3,
                                    long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "memory", "rcx", "r11"
    );
    return ret;
}

/* ── Mach task self (special: no args, returns port name) ───────────────── */

static inline mach_port_t julius_mach_task_self(void) {
    return (mach_port_t)julius_syscall0(TRAP_task_self_trap);
}

static inline mach_port_t julius_mach_host_self(void) {
    return (mach_port_t)julius_syscall0(TRAP_host_self_trap);
}

static inline mach_port_t julius_mach_reply_port(void) {
    return (mach_port_t)julius_syscall0(TRAP_mach_reply_port);
}
