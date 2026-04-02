/*
 * libc/src/io.c
 * JULIUS OS — I/O primitives
 *
 * All output ultimately goes through julius_write(), which is a single
 * BSD syscall (write, number 4). We use fd=2 (stderr) for early console
 * output because on XNU, stderr is connected to the serial console via
 * the kernel's early-boot logging path before a full tty is set up.
 */

#include <julius/types.h>
#include <julius/syscall.h>
#include <julius/io.h>

/* ── write / read ───────────────────────────────────────────────────────── */

ssize_t julius_write(int fd, const void *buf, size_t count) {
    return (ssize_t)julius_syscall3(SYS_write,
                                     (long)fd,
                                     (long)buf,
                                     (long)count);
}

ssize_t julius_read(int fd, void *buf, size_t count) {
    return (ssize_t)julius_syscall3(SYS_read,
                                     (long)fd,
                                     (long)buf,
                                     (long)count);
}

/* ── String output helpers ──────────────────────────────────────────────── */

static size_t str_len(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

ssize_t julius_puts_fd(int fd, const char *s) {
    if (!s) return 0;
    size_t len = str_len(s);
    return julius_write(fd, s, len);
}

ssize_t julius_puts(const char *s) {
    return julius_puts_fd(2, s);   /* default: stderr = serial console */
}

/* ── Hex / decimal printers ─────────────────────────────────────────────── */

void julius_put_hex(uint64_t v) {
    static const char digits[] = "0123456789abcdef";
    char buf[18];   /* "0x" + 16 hex chars */
    int  i = 17;
    buf[17] = '\0';
    if (v == 0) {
        julius_puts("0x0");
        return;
    }
    while (v && i > 1) {
        buf[--i] = digits[v & 0xf];
        v >>= 4;
    }
    buf[--i] = 'x';
    buf[--i] = '0';
    julius_puts(buf + i);
}

void julius_put_dec(int64_t v) {
    char buf[22];
    int  i = 21;
    int  neg = 0;
    buf[21] = '\0';
    if (v < 0) { neg = 1; v = -v; }
    if (v == 0) { julius_puts("0"); return; }
    while (v && i > 0) {
        buf[--i] = '0' + (int)(v % 10);
        v /= 10;
    }
    if (neg) buf[--i] = '-';
    julius_puts(buf + i);
}

/* ── Panic ──────────────────────────────────────────────────────────────── */

void julius_panic(const char *msg) {
    julius_puts("\n!!! JULIUS PANIC: ");
    julius_puts(msg);
    julius_puts("\n");
    /* Halt: invoke the exit syscall with a distinctive code (0xDEAD).
       On a real kernel panic path you'd also trigger a debugger trap. */
    __asm__ volatile ("ud2");   /* undefined instruction = guaranteed fault */
    for (;;) ;                  /* unreachable, but silences noreturn warn */
}
