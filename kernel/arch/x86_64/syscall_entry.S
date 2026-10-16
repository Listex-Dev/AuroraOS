/*
 * AuroraOS Fast Syscall Entry for x86-64
 * Handles 'syscall' instruction via MSR_LSTAR and returns to Ring 3 via iretq
 */

.section .data
.align 16
user_rsp_scratch:
    .quad 0

.section .text
.global syscall_entry
.global syscall_return_to_user
.global fork_ret_trampoline
.extern kernel_tss
.extern syscall_dispatch
.extern syscall_save_regs

syscall_entry:
    /* 1. Save user RSP and switch to per-task kernel stack */
    movq %rsp, user_rsp_scratch(%rip)
    movq kernel_tss+4(%rip), %rsp

    /* 2. Construct struct pt_regs on kernel stack */
    pushq $0x1B                    /* ss (User SS 0x18 | 3) */
    pushq user_rsp_scratch(%rip)   /* rsp (User RSP) */
    pushq %r11                     /* rflags (User RFLAGS saved by syscall) */
    pushq $0x23                    /* cs (User CS 0x20 | 3) */
    pushq %rcx                     /* rip (User RIP saved by syscall) */
    pushq %rax                     /* orig_rax */
    pushq %rdi                     /* rdi */
    pushq %rsi                     /* rsi */
    pushq %rdx                     /* rdx */
    pushq %r8                      /* r8 */
    pushq %r9                      /* r9 */
    pushq %r10                     /* r10 */
    pushq %r11                     /* r11 */
    pushq %rbx                     /* rbx */
    pushq %rbp                     /* rbp */
    pushq %r12                     /* r12 */
    pushq %r13                     /* r13 */
    pushq %r14                     /* r14 */
    pushq %r15                     /* r15 */

    /* Save regs pointer in current task struct */
    movq %rsp, %rdi
    call syscall_save_regs

    /*
     * 3. Map Linux syscall arguments into System V ABI for syscall_dispatch:
     * Linux:   num=RAX, a1=RDI, a2=RSI, a3=RDX, a4=R10, a5=R8, a6=R9
     * SystemV: arg1=RDI, arg2=RSI, arg3=RDX, arg4=RCX, arg5=R8, arg6=R9, arg7=[stack]
     */
    movq 64(%rsp), %rax            /* a6 from r9 slot */
    pushq %rax                     /* 7th parameter on stack: a6 */

    movq 104+8(%rsp), %rdi         /* 1st parameter: num */
    movq 96+8(%rsp), %rsi          /* 2nd parameter: a1 */
    movq 88+8(%rsp), %rdx          /* 3rd parameter: a2 */
    movq 80+8(%rsp), %rcx          /* 4th parameter: a3 */
    movq 56+8(%rsp), %r8           /* 5th parameter: a4 */
    movq 72+8(%rsp), %r9           /* 6th parameter: a5 */

    cld
    call syscall_dispatch

    addq $8, %rsp                  /* remove 7th argument */

    /* Save return value into saved orig_rax slot */
    movq %rax, 104(%rsp)

syscall_return_to_user:
    /* Restore registers from struct pt_regs */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rax                      /* return value */

    /* Atomically return to User Space (Ring 3) */
    iretq

fork_ret_trampoline:
    /* Child process enters here upon first schedule from cpu_switch_to */
    jmp syscall_return_to_user
