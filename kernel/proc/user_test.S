/*
 * User Space Ring 3 Test Program Payload and Trampoline
 */

.section .rodata
.global user_blob_start
.global user_blob_end

user_blob_start:
    /* 1. sys_getpid (39) */
    mov $39, %rax
    syscall

    /* 2. sys_brk(0) (12) */
    mov $12, %rax
    xor %rdi, %rdi
    syscall

    /* 3. sys_write(1, msg, len) (1) */
    mov $1, %rax
    mov $1, %rdi
    lea user_msg(%rip), %rsi
    mov $user_msg_len, %rdx
    syscall

    /* 4. sys_exit(42) (60) */
    mov $60, %rax
    mov $42, %rdi
    syscall

    /* Fallback halt loop */
1:
    jmp 1b

user_msg:
    .ascii "[USER] Hello from Ring 3 (x86-64 syscall)!\n"
user_msg_end:
    .set user_msg_len, user_msg_end - user_msg
user_blob_end:

.section .text
.global enter_usermode
/*
 * void enter_usermode(uint64_t rip, uint64_t rsp);
 * RDI = target RIP, RSI = target RSP
 */
enter_usermode:
    /* Build iretq frame for Ring 3 transition */
    pushq $0x1B                /* User Data Segment: 0x18 | RPL 3 */
    pushq %rsi                 /* User Stack Pointer (RSP) */
    pushq $0x202               /* RFLAGS: IF (Interrupt Enable) | bit 1 reserved */
    pushq $0x23                /* User Code Segment: 0x20 | RPL 3 */
    pushq %rdi                 /* User Instruction Pointer (RIP) */

    /* Clear scratch registers before entering user space */
    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    xorq %rdi, %rdi
    xorq %rbp, %rbp
    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10
    xorq %r11, %r11
    xorq %r12, %r12
    xorq %r13, %r13
    xorq %r14, %r14
    xorq %r15, %r15

    /* Atomically switch to Ring 3 */
    iretq
