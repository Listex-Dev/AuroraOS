/*
 * Assembly ISR stubs for AuroraOS x86-64 IDT
 */

.altmacro
.section .text

.extern exception_dispatch

.macro ISR_NOERRCODE num
.global isr_stub_\num
isr_stub_\num:
    pushq $0                 /* Dummy error code */
    pushq $\num              /* Interrupt vector number */
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
.global isr_stub_\num
isr_stub_\num:
    pushq $\num              /* Interrupt vector number (CPU already pushed error code) */
    jmp isr_common_stub
.endm

/* CPU Exceptions (0-31) */
ISR_NOERRCODE 0   /* #DE Divide Error */
ISR_NOERRCODE 1   /* #DB Debug */
ISR_NOERRCODE 2   /* #NMI Non-Maskable Interrupt */
ISR_NOERRCODE 3   /* #BP Breakpoint */
ISR_NOERRCODE 4   /* #OF Overflow */
ISR_NOERRCODE 5   /* #BR Bound Range Exceeded */
ISR_NOERRCODE 6   /* #UD Invalid Opcode */
ISR_NOERRCODE 7   /* #NM Device Not Available */
ISR_ERRCODE   8   /* #DF Double Fault (has error code) */
ISR_NOERRCODE 9   /* Coprocessor Segment Overrun */
ISR_ERRCODE   10  /* #TS Invalid TSS */
ISR_ERRCODE   11  /* #NP Segment Not Present */
ISR_ERRCODE   12  /* #SS Stack-Segment Fault */
ISR_ERRCODE   13  /* #GP General Protection Fault */
ISR_ERRCODE   14  /* #PF Page Fault */
ISR_NOERRCODE 15  /* Reserved */
ISR_NOERRCODE 16  /* #MF x87 FPU Error */
ISR_ERRCODE   17  /* #AC Alignment Check */
ISR_NOERRCODE 18  /* #MC Machine Check */
ISR_NOERRCODE 19  /* #XM SIMD Floating-Point Exception */
ISR_NOERRCODE 20  /* #VE Virtualization Exception */
ISR_ERRCODE   21  /* #CP Control Protection Exception */
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28  /* #HV Hypervisor Injection Exception */
ISR_ERRCODE   29  /* #VC VMM Communication Exception */
ISR_ERRCODE   30  /* #SX Security Exception */
ISR_NOERRCODE 31

/* Standard stubs for vectors 32 to 255 (IRQs and system calls) */
.macro GEN_STUB idx
    ISR_NOERRCODE \idx
.endm

.set i, 32
.rept 224
    GEN_STUB %i
    .set i, i + 1
.endr

/* Common ISR stub */
isr_common_stub:
    /* Push general purpose registers matching struct interrupt_frame */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* First argument to C function (RDI): pointer to interrupt_frame */
    movq %rsp, %rdi

    /* Direction flag forward for System V AMD64 ABI */
    cld
    call exception_dispatch

    /* Restore general purpose registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    /* Remove int_no and error_code from stack */
    addq $16, %rsp

    /* Return from interrupt */
    iretq

/* Table of all 256 ISR stub pointers */
.section .rodata
.global isr_stub_table
.align 8
isr_stub_table:

.macro GEN_TABLE_ENTRY idx
    .quad isr_stub_\idx
.endm

.set j, 0
.rept 256
    GEN_TABLE_ENTRY %j
    .set j, j + 1
.endr
