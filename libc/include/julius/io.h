/*
 * julius/io.h
 * JULIUS OS — I/O interface declarations
 */

#pragma once
#include <julius/types.h>

/* Raw write/read to a file descriptor */
ssize_t julius_write(int fd, const void *buf, size_t count);
ssize_t julius_read(int fd, void *buf, size_t count);

/* String helpers that call julius_write */
ssize_t julius_puts(const char *s);           /* write to stderr (fd 2) */
ssize_t julius_puts_fd(int fd, const char *s);
void    julius_put_hex(uint64_t v);           /* print 0x... to stderr */
void    julius_put_dec(int64_t v);

/* Panic: print message + halt */
void julius_panic(const char *msg) NORETURN;
