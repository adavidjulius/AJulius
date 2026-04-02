/*
 * libc/src/mem.c
 * JULIUS OS — Memory management
 *
 * This implements a minimal bump-pointer heap backed by mach_vm_allocate.
 * The design is intentionally simple — it exists to support the init process,
 * not as a general-purpose allocator. A future phase will replace this with
 * a proper slab/buddy allocator.
 *
 * mach_vm_allocate Mach trap:
 *   Trap number: -10  (0xFFFFFFF6 on x86_64)
 *   Args: task_port, &address, size, flags
 *   Returns: kern_return_t (0 = success)
 *
 * mach_vm_deallocate Mach trap:
 *   Trap number: -12
 */

#include <julius/types.h>
#include <julius/syscall.h>
#include <julius/mem.h>
#include <julius/io.h>

/* ── mach_vm_allocate / deallocate traps ────────────────────────────────── */

#define TRAP_mach_vm_allocate   MACH_TRAP(10)
#define TRAP_mach_vm_deallocate MACH_TRAP(12)

kern_return_t julius_vm_allocate(mach_port_t task,
                                  mach_vm_address_t *addr,
                                  mach_vm_size_t size,
                                  int flags) {
    /*
     * mach_vm_allocate takes the address as a pointer-to-pointer so the
     * kernel can write back the chosen address. We pass it as a uintptr_t
     * pointing to our local variable.
     */
    return (kern_return_t)julius_syscall4(
        TRAP_mach_vm_allocate,
        (long)task,
        (long)addr,
        (long)size,
        (long)flags
    );
}

kern_return_t julius_vm_deallocate(mach_port_t task,
                                    mach_vm_address_t addr,
                                    mach_vm_size_t size) {
    return (kern_return_t)julius_syscall3(
        TRAP_mach_vm_deallocate,
        (long)task,
        (long)addr,
        (long)size
    );
}

/* ── Bump-pointer heap ──────────────────────────────────────────────────── */

#define HEAP_INITIAL_SIZE   (1UL * 1024 * 1024)    /* 1 MiB */
#define HEAP_ALIGN          16                       /* bytes */

static char *heap_base = NULL;
static char *heap_end  = NULL;
static char *heap_ptr  = NULL;

void julius_mem_init(void) {
    mach_vm_address_t addr = 0;
    kern_return_t kr = julius_vm_allocate(
        julius_mach_task_self(),
        &addr,
        HEAP_INITIAL_SIZE,
        VM_FLAGS_ANYWHERE
    );
    if (kr != KERN_SUCCESS) {
        julius_panic("mem_init: mach_vm_allocate failed");
    }
    heap_base = heap_ptr = (char *)(uintptr_t)addr;
    heap_end  = heap_base + HEAP_INITIAL_SIZE;
    julius_puts("mem: heap initialised at ");
    julius_put_hex((uint64_t)(uintptr_t)heap_base);
    julius_puts(", size 1 MiB\n");
}

void *julius_malloc(size_t size) {
    if (UNLIKELY(!heap_base)) julius_mem_init();
    /* Round up to alignment */
    size = ALIGN(size, HEAP_ALIGN);
    if (UNLIKELY(heap_ptr + size > heap_end)) {
        /* Extend heap by another region */
        mach_vm_address_t addr = 0;
        size_t extend = size < HEAP_INITIAL_SIZE ? HEAP_INITIAL_SIZE : size * 2;
        kern_return_t kr = julius_vm_allocate(
            julius_mach_task_self(), &addr, extend, VM_FLAGS_ANYWHERE
        );
        if (kr != KERN_SUCCESS) return NULL;
        heap_ptr = (char *)(uintptr_t)addr;
        heap_end = heap_ptr + extend;
    }
    void *p = heap_ptr;
    heap_ptr += size;
    return p;
}

void julius_free(void *ptr) {
    /* Bump allocator: free is a no-op. Future: implement free-list. */
    (void)ptr;
}

void *julius_calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = julius_malloc(total);
    if (p) julius_memset(p, 0, total);
    return p;
}

/* ── String / memory primitives ─────────────────────────────────────────── */

size_t julius_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void *julius_memcpy(void *dst, const void *src, size_t n) {
    char       *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *julius_memset(void *dst, int c, size_t n) {
    char *d = (char *)dst;
    while (n--) *d++ = (char)c;
    return dst;
}

int julius_memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

int julius_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

char *julius_strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}
