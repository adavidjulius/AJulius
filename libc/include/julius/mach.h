/*
 * julius/mach.h
 * JULIUS OS — Mach IPC interface
 *
 * Provides the minimum Mach message and port primitives needed by
 * julius-init to establish the bootstrap port and communicate with
 * child processes. This replaces the Apple-provided mach/ headers.
 */

#pragma once
#include <julius/types.h>

/* ── mach_msg structures (from osfmk/mach/message.h) ───────────────────── */

typedef uint32_t mach_msg_id_t;
typedef uint32_t mach_msg_option_t;

#define MACH_MSGH_BITS(remote, local)   ((remote) | ((local) << 8))
#define MACH_MSGH_BITS_REMOTE(bits)     ((bits) & 0xff)
#define MACH_MSGH_BITS_LOCAL(bits)      (((bits) >> 8) & 0xff)

/* mach_msg options */
#define MACH_SEND_MSG           0x00000001
#define MACH_RCV_MSG            0x00000002
#define MACH_RCV_LARGE          0x00000004
#define MACH_SEND_TIMEOUT       0x00000010
#define MACH_RCV_TIMEOUT        0x00000100
#define MACH_MSG_TIMEOUT_NONE   0

typedef struct mach_msg_header {
    mach_msg_bits_t   msgh_bits;
    mach_msg_size_t   msgh_size;
    mach_port_t       msgh_remote_port;
    mach_port_t       msgh_local_port;
    uint32_t          msgh_voucher_port;
    mach_msg_id_t     msgh_id;
} mach_msg_header_t;

/* A simple message with no body (only header) */
typedef struct julius_simple_msg {
    mach_msg_header_t header;
} julius_simple_msg_t;

/* ── mach_msg trap ──────────────────────────────────────────────────────── */

/*
 * mach_msg() is implemented as Mach trap -31.
 * We call it via julius_syscall6 since it takes 6 args.
 */
kern_return_t julius_mach_msg(mach_msg_header_t *msg,
                               mach_msg_option_t  option,
                               mach_msg_size_t    send_size,
                               mach_msg_size_t    rcv_size,
                               mach_port_t        rcv_name,
                               uint32_t           timeout);

/* ── Port management ────────────────────────────────────────────────────── */

/*
 * Allocate a new port right in the calling task.
 * right: MACH_PORT_RIGHT_RECEIVE, MACH_PORT_RIGHT_SEND, etc.
 *
 * Implemented as Mach trap -16 (mach_port_allocate_trap).
 */
kern_return_t julius_port_allocate(mach_port_t task,
                                    mach_port_right_t right,
                                    mach_port_name_t *name);

/*
 * Insert a send right into a port.
 * Implemented via mach_port_insert_right syscall (BSD 0x2000218).
 */
kern_return_t julius_port_insert_right(mach_port_t         task,
                                        mach_port_name_t    name,
                                        mach_port_t         poly,
                                        mach_msg_type_name_t poly_poly);

/*
 * Set the bootstrap port for the current task.
 * Syscall: task_set_bootstrap_port (Mach trap -88 on modern XNU).
 */
kern_return_t julius_task_set_bootstrap_port(mach_port_t task,
                                              mach_port_t bootstrap_port);

/* ── Convenience ────────────────────────────────────────────────────────── */

/* Send a simple headeronly message (no body). Returns KERN_SUCCESS on send. */
kern_return_t julius_send_simple(mach_port_t remote,
                                  mach_msg_id_t msg_id);

/* Block forever waiting to receive any message on port. */
kern_return_t julius_recv_simple(mach_port_t port,
                                  julius_simple_msg_t *out);
