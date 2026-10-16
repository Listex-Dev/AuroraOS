/*
 * Ring 3 User Mode Test for Milestone M7
 * Verifies user-space syscall access to /proc, /dev, and /tmp
 */

.section .rodata
.global user_m7_blob_start
.global user_m7_blob_end

user_m7_blob_start:
    /* 1. sys_open("/proc/version", O_RDONLY = 0, 0) */
    mov $2, %rax                   /* SYS_open */
    lea path_proc_ver(%rip), %rdi
    xor %rsi, %rsi                 /* O_RDONLY */
    xor %rdx, %rdx
    syscall

    cmp $0, %rax
    jl .Lm7_error
    mov %rax, %r8                  /* r8 = fd */

    /* sys_read(fd, stack_buf, 32) */
    mov $0, %rax                   /* SYS_read */
    mov %r8, %rdi
    lea -64(%rsp), %rsi
    mov $32, %rdx
    syscall

    cmp $0, %rax
    jle .Lm7_error

    /* sys_close(fd) */
    mov $3, %rax                   /* SYS_close */
    mov %r8, %rdi
    syscall

    /* 2. sys_open("/dev/zero", O_RDONLY = 0, 0) */
    mov $2, %rax                   /* SYS_open */
    lea path_dev_zero(%rip), %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    syscall

    cmp $0, %rax
    jl .Lm7_error
    mov %rax, %r8

    /* sys_read(fd, stack_buf, 16) */
    mov $0, %rax
    mov %r8, %rdi
    lea -64(%rsp), %rsi
    mov $16, %rdx
    syscall

    cmp $16, %rax
    jne .Lm7_error

    /* sys_close(fd) */
    mov $3, %rax
    mov %r8, %rdi
    syscall

    /* 3. sys_open("/tmp/m7_user.txt", O_CREAT | O_RDWR = 0x42, 0666 = 0x1B6) */
    mov $2, %rax
    lea path_tmp_file(%rip), %rdi
    mov $0x42, %rsi                /* O_CREAT | O_RDWR */
    mov $0x1B6, %rdx               /* 0666 */
    syscall

    cmp $0, %rax
    jl .Lm7_error
    mov %rax, %r8

    /* sys_write(fd, msg, msg_len) */
    mov $1, %rax                   /* SYS_write */
    mov %r8, %rdi
    lea tmp_msg(%rip), %rsi
    mov $tmp_msg_len, %rdx
    syscall

    cmp $tmp_msg_len, %rax
    jne .Lm7_error

    /* sys_close(fd) */
    mov $3, %rax
    mov %r8, %rdi
    syscall

    /* 4. sys_unlink("/tmp/m7_user.txt") */
    mov $87, %rax                  /* SYS_unlink */
    lea path_tmp_file(%rip), %rdi
    syscall

    cmp $0, %rax
    jne .Lm7_error

    /* Exit with success code 77 */
    mov $60, %rax                  /* SYS_exit */
    mov $77, %rdi                  /* status = 77 */
    syscall

.Lm7_error:
    mov $60, %rax
    mov $125, %rdi                 /* error status 125 */
    syscall

.Lm7_loop:
    jmp .Lm7_loop

path_proc_ver:
    .asciz "/proc/version"

path_dev_zero:
    .asciz "/dev/zero"

path_tmp_file:
    .asciz "/tmp/m7_user.txt"

tmp_msg:
    .ascii "M7_USER_OK\n"
tmp_msg_end:
    .set tmp_msg_len, tmp_msg_end - tmp_msg

user_m7_blob_end:
