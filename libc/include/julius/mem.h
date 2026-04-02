/*
 * julius/mem.h
 * JULIUS OS — Memory allocation interface
 *
 * Backed by mach_vm_allocate() / mach_vm_deallocate() Mach traps.
 * The allocator is a simple bump-pointer slab over a 1 MiB initial
 * region obtained from the VM subsystem. It is intentionally minimal —
 * suitable for an init process, not for a general-purpose heap.
 */

#pragma once
#include <julius/types.h>

/* Initialise the heap. Called once from julius_start(). */
void julius_mem_init(void);

/* Allocate size bytes, 16-byte aligned. Returns NULL on failure. */
void *julius_malloc(size_t size);

/* Free is a no-op in the bump allocator; included for ABI completeness. */
void julius_free(void *ptr);

/* Zeroing allocator */
void *julius_calloc(size_t nmemb, size_t size);

/* Wrappers around mach_vm_allocate / mach_vm_deallocate */
kern_return_t julius_vm_allocate(mach_port_t task,
                                  mach_vm_address_t *addr,
                                  mach_vm_size_t size,
                                  int flags);

kern_return_t julius_vm_deallocate(mach_port_t task,
                                    mach_vm_address_t addr,
                                    mach_vm_size_t size);

/* String ops (no libc) */
size_t  julius_strlen(const char *s);
void   *julius_memcpy(void *dst, const void *src, size_t n);
void   *julius_memset(void *dst, int c, size_t n);
int     julius_memcmp(const void *a, const void *b, size_t n);
int     julius_strcmp(const char *a, const char *b);
char   *julius_strcpy(char *dst, const char *src);
