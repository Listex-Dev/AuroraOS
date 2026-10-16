/*
 * User Space Test Code for Milestone M5
 * Runs in Ring 3: performs sys_fork, child execve("/bin/hello"), parent wait4
 */

.section .rodata
.global user_m5_blob_start
.global user_m5_blob_end

user_m5_blob_start:
    /* 1. sys_fork() (SYS_fork = 57) */
    mov $57, %rax
    syscall

    /* Test return value */
    cmp $0, %rax
    jl .Lfork_error
    je .Lchild_code

.Lparent_code:
    /* Parent process: %rax = child PID */
    mov %rax, %r8              /* Save child PID in %r8 */

    /* 2. sys_wait4(child_pid, &status, 0, NULL) (SYS_wait4 = 61) */
    mov $61, %rax              /* SYS_wait4 */
    mov %r8, %rdi              /* arg1: child pid */
    lea -16(%rsp), %rsi        /* arg2: &status on user stack */
    xor %rdx, %rdx             /* arg3: options = 0 */
    xor %r10, %r10             /* arg4: rusage = NULL */
    syscall

    /* %rax should equal child PID */
    cmp %r8, %rax
    jne .Lwait_error

    /* Verify exit status code 77 in high byte: status == (77 << 8) */
    mov -16(%rsp), %ebx
    shr $8, %ebx
    cmp $77, %ebx
    jne .Lstatus_error

    /* Success! Exit with code 99 */
    mov $60, %rax              /* SYS_exit */
    mov $99, %rdi              /* exit code 99 = SUCCESS */
    syscall
    jmp .Lhalt

.Lchild_code:
    /* Child process: sys_execve("/bin/hello", NULL, NULL) (SYS_execve = 59) */
    mov $59, %rax
    lea hello_path(%rip), %rdi /* filename = "/bin/hello" */
    xor %rsi, %rsi             /* argv = NULL */
    xor %rdx, %rdx             /* envp = NULL */
    syscall

    /* If execve returns, it failed! Exit with error 111 */
    mov $60, %rax
    mov $111, %rdi
    syscall
    jmp .Lhalt

.Lfork_error:
    mov $60, %rax
    mov $101, %rdi
    syscall
    jmp .Lhalt

.Lwait_error:
    mov $60, %rax
    mov $102, %rdi
    syscall
    jmp .Lhalt

.Lstatus_error:
    mov $60, %rax
    mov $103, %rdi
    syscall
    jmp .Lhalt

.Lhalt:
    jmp .Lhalt

hello_path:
    .asciz "/bin/hello"

user_m5_blob_end:
