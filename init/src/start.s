/*
 * init/src/start.s
 * JULIUS OS — Raw entry point for julius-init (PID 1)
 *
 * XNU exec ABI (x86_64):
 *   At process entry, %rsp points to:
 *     [rsp+0]   argc         (= 0 for PID 1 early boot)
 *     [rsp+8]   argv[0]      (pointer to program name string)
 *     ...
 *     envp[], apple[] strings follow
 *
 *   The stack is already 16-byte aligned at the OS entry point
 *   (the kernel does this before jumping here).
 *
 * We zero rbp to mark the outermost stack frame (required for stack
 * unwinders and debuggers), then call julius_start() in main.c.
 * julius_start() never returns; if it somehow does we execute the
 * exit syscall to prevent a wild return.
 */

    .text
    .global _start
    .align  4

_start:
    xorq    %rbp, %rbp          /* mark outermost frame: no caller */
    andq    $-16, %rsp          /* ensure 16-byte stack alignment   */

    /*
     * XNU places argc in [rsp] and argv in [rsp+8].
     * We pass them to julius_start for completeness, even though
     * PID 1 typically gets argc=0 from the kernel.
     */
    movq    (%rsp),   %rdi      /* arg1: argc */
    leaq    8(%rsp),  %rsi      /* arg2: argv (pointer to first entry) */

    callq   _julius_start

    /* julius_start() is NORETURN; defensive exit if we get here */
    movq    $0x2000001, %rax    /* SYS_exit (BSD class) */
    xorq    %rdi, %rdi          /* exit code = 0 */
    syscall

    /* absolute fallback: undefined instruction trap */
    ud2
