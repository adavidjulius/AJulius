/*
 * init/src/main.c
 * JULIUS OS — julius-init: The First User-Space Process (PID 1)
 *
 * This is the root of JULIUS OS user-space. The kernel (XNU) execs this
 * binary directly after the kernel bootstraps. It is responsible for:
 *
 *   1. Printing the JULIUS OS banner to the serial console (fd 2).
 *   2. Initialising the heap (mach_vm_allocate-backed bump allocator).
 *   3. Establishing the Mach bootstrap port that child processes will
 *      inherit and use to look up OS services.
 *   4. Entering a Mach message receive loop (the "service loop") so
 *      the process never exits — a kernel panic results if PID 1 exits.
 *
 * IMPORTANT: PID 1 must NEVER return from julius_start() or call exit().
 *
 * Compile: see build/build-init.sh
 */

#include <julius/types.h>
#include <julius/io.h>
#include <julius/mem.h>
#include <julius/mach.h>
#include <julius/syscall.h>

/* ── Banner ─────────────────────────────────────────────────────────────── */

static const char *BANNER =
    "\r\n"
    "  ██╗██╗   ██╗██╗     ██╗██╗   ██╗███████╗     ██████╗ ███████╗\r\n"
    "  ██║██║   ██║██║     ██║██║   ██║██╔════╝    ██╔═══██╗██╔════╝\r\n"
    "  ██║██║   ██║██║     ██║██║   ██║███████╗    ██║   ██║███████╗\r\n"
    "  ██║██║   ██║██║     ██║██║   ██║╚════██║    ██║   ██║╚════██║\r\n"
    "  ██║╚██████╔╝███████╗██║╚██████╔╝███████║    ╚██████╔╝███████║\r\n"
    "  ╚═╝ ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚══════╝     ╚═════╝ ╚══════╝\r\n"
    "\r\n"
    "  JULIUS OS — Custom XNU-based Operating System\r\n"
    "  Built from scratch. No Apple user-space.\r\n"
    "\r\n";

/* ── Mach message IDs used by the julius bootstrap protocol ─────────────── */

#define JULIUS_MSG_PING         0x4A4C0001  /* 'JL' + 1 */
#define JULIUS_MSG_REGISTER     0x4A4C0002
#define JULIUS_MSG_LOOKUP       0x4A4C0003

/* ── Bootstrap port setup ───────────────────────────────────────────────── */

static mach_port_t g_bootstrap_port = MACH_PORT_NULL;

static void setup_bootstrap_port(void) {
    kern_return_t kr;
    mach_port_t   self = julius_mach_task_self();
    mach_port_name_t port_name;

    julius_puts("init: allocating bootstrap receive port...\r\n");

    /* Step 1: Allocate a RECEIVE right — this is the port we own */
    kr = julius_port_allocate(self, MACH_PORT_RIGHT_RECEIVE, &port_name);
    if (kr != KERN_SUCCESS) {
        julius_puts("init: FATAL: port_allocate failed, kr=");
        julius_put_hex((uint64_t)(uint32_t)kr);
        julius_puts("\r\n");
        julius_panic("cannot allocate bootstrap port");
    }

    julius_puts("init: bootstrap receive port = ");
    julius_put_hex((uint64_t)port_name);
    julius_puts("\r\n");

    /* Step 2: Add a SEND right to the same port so others can send to us */
    kr = julius_port_insert_right(self, port_name, port_name,
                                   MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
        julius_puts("init: FATAL: port_insert_right failed, kr=");
        julius_put_hex((uint64_t)(uint32_t)kr);
        julius_puts("\r\n");
        julius_panic("cannot insert send right");
    }

    /* Step 3: Register as the task's bootstrap port — all child processes
       that fork from us will inherit a send right to this port */
    kr = julius_task_set_bootstrap_port(self, (mach_port_t)port_name);
    if (kr != KERN_SUCCESS) {
        julius_puts("init: WARNING: task_set_bootstrap_port failed, kr=");
        julius_put_hex((uint64_t)(uint32_t)kr);
        julius_puts("\r\n");
        /* Non-fatal — we still run, just without the formal bootstrap port */
    }

    g_bootstrap_port = (mach_port_t)port_name;
    julius_puts("init: bootstrap port established\r\n");
}

/* ── Service dispatch loop ──────────────────────────────────────────────── */

/*
 * The init service loop receives Mach messages on the bootstrap port
 * and dispatches them. This is the "event loop" of PID 1.
 *
 * In Phase 2 of development, this will grow to handle:
 *   - Service registration (child daemons registering their ports)
 *   - Service lookup (clients asking for daemon ports by name)
 *   - Process reaping (wait for zombie children)
 *
 * For now it just prints the message ID and ACKs with a simple reply.
 */

static void service_loop(void) {
    julius_puts("init: entering service loop (waiting for Mach messages)\r\n");

    julius_simple_msg_t msg;

    for (;;) {
        kern_return_t kr = julius_recv_simple(g_bootstrap_port, &msg);

        if (kr != KERN_SUCCESS) {
            julius_puts("init: mach_msg recv error, kr=");
            julius_put_hex((uint64_t)(uint32_t)kr);
            julius_puts("\r\n");
            /* Don't panic — just keep looping. The kernel may send us
               internal notifications that we don't fully understand yet. */
            continue;
        }

        julius_puts("init: received msg id=");
        julius_put_hex((uint64_t)msg.header.msgh_id);
        julius_puts(" from port=");
        julius_put_hex((uint64_t)msg.header.msgh_remote_port);
        julius_puts("\r\n");

        switch (msg.header.msgh_id) {
            case JULIUS_MSG_PING:
                julius_puts("init: PING received, sending PONG\r\n");
                if (msg.header.msgh_remote_port != MACH_PORT_NULL) {
                    julius_send_simple(msg.header.msgh_remote_port,
                                        JULIUS_MSG_PING + 0x8000);
                }
                break;

            default:
                julius_puts("init: unknown message, ignoring\r\n");
                break;
        }
    }
    /* Never reached */
}

/* ── Entry point ────────────────────────────────────────────────────────── */

/*
 * julius_start() is called from start.s. It receives argc/argv from the
 * kernel's exec ABI but typically these are 0/empty for PID 1.
 *
 * This function MUST NEVER RETURN.
 */
void julius_start(int argc, char **argv) NORETURN;

void julius_start(int argc, char **argv) {
    (void)argc; (void)argv;

    /* 1. Print banner — first user-space output of JULIUS OS */
    julius_puts(BANNER);
    julius_puts("init: PID 1 starting\r\n");

    /* 2. Initialise the heap */
    julius_mem_init();

    /* 3. Confirm we have a task port */
    mach_port_t self = julius_mach_task_self();
    julius_puts("init: mach_task_self() = ");
    julius_put_hex((uint64_t)self);
    julius_puts("\r\n");

    /* 4. Set up the Mach bootstrap port */
    setup_bootstrap_port();

    /* 5. Quick self-test: malloc a small buffer and check it */
    {
        char *buf = (char *)julius_malloc(64);
        if (!buf) julius_panic("self-test: malloc returned NULL");
        julius_memset(buf, 'J', 63);
        buf[63] = '\0';
        julius_puts("init: malloc self-test OK (64 bytes at ");
        julius_put_hex((uint64_t)(uintptr_t)buf);
        julius_puts(")\r\n");
    }

    julius_puts("\r\ninit: all systems nominal. JULIUS OS is alive.\r\n\r\n");

    /* 6. Enter the service loop — never returns */
    service_loop();

    /* Defensive: should be unreachable */
    __builtin_unreachable();
}
