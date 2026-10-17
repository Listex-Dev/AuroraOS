.code64
.section .rodata
.global user_m9_blob_start
.global user_m9_blob_end

user_m9_blob_start:
    /* 1. Open /dev/tty1 (O_RDWR = 2) */
    mov $2, %rax                   /* SYS_open */
    lea tty1_path(%rip), %rdi
    mov $2, %rsi                   /* O_RDWR */
    xor %rdx, %rdx                 /* mode = 0 */
    syscall

    cmp $0, %rax
    jl fail_open
    mov %rax, %r14                 /* Save fd in r14 */

    /* 2. Test ioctl TCGETS (0x5401) with stack buffer */
    mov $16, %rax                  /* SYS_ioctl */
    mov %r14, %rdi
    mov $0x5401, %rsi              /* TCGETS */
    lea -128(%rsp), %rdx           /* termios buffer on stack */
    syscall

    cmp $0, %rax
    jne fail_ioctl

    /* 3. Fork child worker process */
    mov $57, %rax                  /* SYS_fork */
    syscall

    cmp $0, %rax
    jl fail_fork
    je child_worker

    /* Parent Process */
    mov %rax, %r15                 /* Save child PID in r15 */

    /* Wait for child to be terminated by SIGINT */
    mov $61, %rax                  /* SYS_wait4 */
    mov %r15, %rdi                 /* pid = child_pid */
    lea -32(%rsp), %rsi            /* status buffer on stack */
    xor %rdx, %rdx                 /* options = 0 */
    xor %r10, %r10                /* rusage = NULL */
    syscall

    cmp %r15, %rax
    jne fail_wait

    /* Check that child was terminated by SIGINT (signal 2, status & 0x7F == 2) */
    mov -32(%rsp), %eax
    and $0x7F, %eax
    cmp $2, %eax
    jne fail_signal

    /* Success! Exit with code 89 */
    mov $60, %rax                  /* SYS_exit */
    mov $89, %rdi
    syscall

child_worker:
    /* Child: getpid */
    mov $39, %rax                  /* SYS_getpid */
    syscall
    mov %rax, -32(%rsp)            /* Store child PID on stack */

    /* Set child process as foreground process group on tty1: TIOCSPGRP (0x5410) */
    mov $16, %rax                  /* SYS_ioctl */
    mov %r14, %rdi
    mov $0x5410, %rsi              /* TIOCSPGRP */
    lea -32(%rsp), %rdx            /* Pointer to pid on stack */
    syscall

child_loop:
    /* Infinite loop awaiting SIGINT from TTY line discipline */
    pause
    jmp child_loop

fail_open:
    mov $60, %rax
    mov $1, %rdi
    syscall

fail_ioctl:
    mov $60, %rax
    mov $2, %rdi
    syscall

fail_fork:
    mov $60, %rax
    mov $3, %rdi
    syscall

fail_wait:
    mov $60, %rax
    mov $4, %rdi
    syscall

fail_signal:
    mov $60, %rax
    mov $5, %rdi
    syscall

tty1_path:
    .asciz "/dev/tty1"

user_m9_blob_end:
