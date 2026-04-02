/*
 * julius/types.h
 * JULIUS OS — Fundamental type definitions
 *
 * This header defines the basic types used across julius-libc and julius-init.
 * It avoids any dependency on system headers.
 */

#pragma once

/* ── Integer types ──────────────────────────────────────────────────────── */

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint64_t           uintptr_t;
typedef int64_t            intptr_t;
typedef uint64_t           size_t;
typedef int64_t            ssize_t;
typedef int64_t            off_t;
typedef int32_t            pid_t;
typedef uint32_t           uid_t;
typedef uint32_t           gid_t;
typedef int32_t            kern_return_t;

/* ── Boolean ────────────────────────────────────────────────────────────── */

typedef int                bool;
#define true               1
#define false              0

/* ── NULL ───────────────────────────────────────────────────────────────── */

#define NULL               ((void *)0)

/* ── Mach types ─────────────────────────────────────────────────────────── */

typedef uint32_t           mach_port_t;
typedef uint32_t           mach_port_name_t;
typedef uint32_t           mach_port_right_t;
typedef uint32_t           mach_msg_type_name_t;
typedef uint32_t           mach_msg_size_t;
typedef uint32_t           mach_msg_bits_t;
typedef uint64_t           mach_vm_address_t;
typedef uint64_t           mach_vm_size_t;
typedef uint32_t           vm_prot_t;

#define MACH_PORT_NULL     ((mach_port_t)0)
#define KERN_SUCCESS       ((kern_return_t)0)
#define KERN_FAILURE       ((kern_return_t)5)

/* Mach port right types (from mach/port.h) */
#define MACH_PORT_RIGHT_SEND            ((mach_port_right_t)0)
#define MACH_PORT_RIGHT_RECEIVE         ((mach_port_right_t)1)
#define MACH_PORT_RIGHT_SEND_ONCE       ((mach_port_right_t)2)

/* mach_msg type names */
#define MACH_MSG_TYPE_MAKE_SEND         ((mach_msg_type_name_t)20)
#define MACH_MSG_TYPE_MOVE_SEND         ((mach_msg_type_name_t)17)
#define MACH_MSG_TYPE_COPY_SEND         ((mach_msg_type_name_t)19)

/* VM flags */
#define VM_FLAGS_ANYWHERE               1
#define VM_PROT_READ                    ((vm_prot_t)0x01)
#define VM_PROT_WRITE                   ((vm_prot_t)0x02)
#define VM_PROT_EXECUTE                 ((vm_prot_t)0x04)
#define VM_PROT_DEFAULT                 (VM_PROT_READ | VM_PROT_WRITE)

/* ── Compiler helpers ───────────────────────────────────────────────────── */

#define NORETURN               __attribute__((noreturn))
#define UNUSED                 __attribute__((unused))
#define PACKED                 __attribute__((packed))
#define ALWAYS_INLINE          __attribute__((always_inline)) static inline
#define LIKELY(x)              __builtin_expect(!!(x), 1)
#define UNLIKELY(x)            __builtin_expect(!!(x), 0)
#define ARRAY_SIZE(a)          (sizeof(a) / sizeof((a)[0]))
#define ALIGN(n, a)            (((n) + (a) - 1) & ~((a) - 1))
