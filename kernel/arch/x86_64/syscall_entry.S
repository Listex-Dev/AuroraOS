/*
 * AuroraOS Fast Syscall Entry for x86-64
 * Handles 'syscall' instruction via MSR_LSTAR and returns to Ring 3 via iretq
 */

.section .data
.align 16
user_rsp_save:
    .quad 0

.section .bss
.align 16
syscall_kernel_stack:
    .skip 16384
syscall_kernel_stack_top:

.section .text
.global syscall_entry
.extern syscall_dispatch

syscall_entry:
    /* 1. Save user RSP and switch to kernel syscall stack */
    movq %rsp, user_rsp_save
    movq $syscall_kernel_stack_top, %rsp

    /* 2. Construct iretq frame for return to Ring 3 */
    pushq $0x1B                /* User SS (0x18 | 3) */
    pushq user_rsp_save        /* User RSP */
    pushq %r11                 /* User RFLAGS (saved in R11 by syscall) */
    pushq $0x23                /* User CS (0x20 | 3) */
    pushq %rcx                 /* User RIP (saved in RCX by syscall) */

    /* 3. Save callee-saved registers */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /*
     * 4. Map Linux syscall arguments into System V ABI:
     * Linux:   num=RAX, a1=RDI, a2=RSI, a3=RDX, a4=R10, a5=R8, a6=R9
     * SystemV: arg1=RDI, arg2=RSI, arg3=RDX, arg4=RCX, arg5=R8, arg6=R9, arg7=[stack]
     */
    pushq %r9                  /* 7th parameter on stack: a6 */
    movq %r8, %r9              /* 6th parameter: a5 */
    movq %r10, %r8             /* 5th parameter: a4 */
    movq %rdx, %rcx            /* 4th parameter: a3 */
    movq %rsi, %rdx            /* 3rd parameter: a2 */
    movq %rdi, %rsi            /* 2nd parameter: a1 */
    movq %rax, %rdi            /* 1st parameter: num */

    cld
    call syscall_dispatch

    /* Clean up 7th argument */
    addq $8, %rsp

    /* Restore callee-saved registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    /*
     * 5. Atomically return to User Space (Ring 3).
     * RAX contains the return value from syscall_dispatch.
     */
    iretq
