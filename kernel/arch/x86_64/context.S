/*
 * AuroraOS Context Switch and Task Trampoline (x86-64)
 */

.section .text

.global cpu_switch_to
.extern tss_set_rsp0
.extern sched_exit

/*
 * void cpu_switch_to(task_t *prev, task_t *next);
 * RDI = prev, RSI = next
 * Offset of rsp in task_struct: 0
 * Offset of kstack in task_struct: 8
 */
cpu_switch_to:
    /* 1. Save callee-saved registers on current stack */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* 2. Save RSP to prev->rsp */
    movq %rsp, 0(%rdi)

    /* 3. Switch to next->rsp */
    movq 0(%rsi), %rsp

    /* 4. Update TSS rsp0 for interrupt stack safety */
    pushq %rsi                  /* Preserve next pointer */
    movq 8(%rsi), %rdi          /* next->kstack */
    addq $16384, %rdi           /* Top of kernel stack (16 KiB) */
    call tss_set_rsp0
    popq %rsi                   /* Restore next pointer */

    /* 5. Restore callee-saved registers from new stack */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    ret

/*
 * Trampoline for newly spawned kernel threads
 * Stack contains entry_point address
 */
.global task_trampoline
task_trampoline:
    /* Pop entry function address into RAX */
    popq %rax

    /* Call thread entry function */
    call *%rax

    /* If thread entry returns, cleanly exit task */
    movq $0, %rdi
    call sched_exit

    /* Should never reach here */
1:
    hlt
    jmp 1b
