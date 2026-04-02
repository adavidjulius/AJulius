/*
 * libc/src/mach.c
 * JULIUS OS — Mach IPC primitives
 *
 * Implements the minimal Mach message and port operations needed by
 * julius-init to establish the bootstrap port and dispatch IPC.
 *
 * Trap numbers are from osfmk/kern/syscall_sw.c in the XNU source.
 */

#include <julius/types.h>
#include <julius/syscall.h>
#include <julius/mach.h>
#include <julius/io.h>

/* ── mach_msg ───────────────────────────────────────────────────────────── */

kern_return_t julius_mach_msg(mach_msg_header_t *msg,
                               mach_msg_option_t  option,
                               mach_msg_size_t    send_size,
                               mach_msg_size_t    rcv_size,
                               mach_port_t        rcv_name,
                               uint32_t           timeout) {
    /*
     * mach_msg_trap is Mach trap -31.
     * Signature: mach_msg(header, option, send_sz, rcv_sz, rcv_port, timeout, notify)
     * We pass 0 for the 7th arg (notify port) as we don't use it here.
     */
    return (kern_return_t)julius_syscall6(
        TRAP_mach_msg_trap,
        (long)msg,
        (long)option,
        (long)send_size,
        (long)rcv_size,
        (long)rcv_name,
        (long)timeout
    );
    /* Note: the 7th arg (notify) is pushed on the stack — we omit it
       and XNU treats it as 0 (MACH_PORT_NULL), which is fine for basic use. */
}

/* ── Port allocation (Mach trap -16) ────────────────────────────────────── */

kern_return_t julius_port_allocate(mach_port_t task,
                                    mach_port_right_t right,
                                    mach_port_name_t *name) {
    return (kern_return_t)julius_syscall3(
        TRAP_mach_port_allocate,
        (long)task,
        (long)right,
        (long)name
    );
}

/* ── Port insert right (BSD syscall 0x2000218 = 536) ────────────────────── */

#define SYS_mach_port_insert_right  BSD_SYSCALL(0x218)

kern_return_t julius_port_insert_right(mach_port_t task,
                                        mach_port_name_t name,
                                        mach_port_t poly,
                                        mach_msg_type_name_t poly_type) {
    return (kern_return_t)julius_syscall4(
        SYS_mach_port_insert_right,
        (long)task,
        (long)name,
        (long)poly,
        (long)poly_type
    );
}

/* ── task_set_bootstrap_port (Mach trap -88) ────────────────────────────── */

#define TRAP_task_set_bootstrap     MACH_TRAP(88)

kern_return_t julius_task_set_bootstrap_port(mach_port_t task,
                                              mach_port_t bootstrap_port) {
    return (kern_return_t)julius_syscall2(
        TRAP_task_set_bootstrap,
        (long)task,
        (long)bootstrap_port
    );
}

/* ── Convenience: send a simple header-only message ─────────────────────── */

kern_return_t julius_send_simple(mach_port_t remote, mach_msg_id_t msg_id) {
    julius_simple_msg_t msg;
    msg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size        = sizeof(msg.header);
    msg.header.msgh_remote_port = remote;
    msg.header.msgh_local_port  = MACH_PORT_NULL;
    msg.header.msgh_voucher_port= 0;
    msg.header.msgh_id          = msg_id;

    return julius_mach_msg(
        &msg.header,
        MACH_SEND_MSG,
        (mach_msg_size_t)sizeof(msg.header),
        0,
        MACH_PORT_NULL,
        MACH_MSG_TIMEOUT_NONE
    );
}

/* ── Convenience: block-receive on a port ───────────────────────────────── */

kern_return_t julius_recv_simple(mach_port_t port,
                                  julius_simple_msg_t *out) {
    return julius_mach_msg(
        &out->header,
        MACH_RCV_MSG,
        0,
        (mach_msg_size_t)sizeof(*out),
        port,
        MACH_MSG_TIMEOUT_NONE
    );
}
