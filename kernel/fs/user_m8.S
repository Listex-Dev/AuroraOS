/*
 * Ring 3 User Mode Test for Milestone M8
 * Verifies user-space syscall access to ext2/ext4 filesystem
 */

.section .rodata
.global user_m8_blob_start
.global user_m8_blob_end

user_m8_blob_start:
    /* 1. sys_open("/test_ext4.txt", O_CREAT | O_RDWR = 0x42, 0666 = 0x1B6) */
    mov $2, %rax                   /* SYS_open */
    lea path_ext4_file(%rip), %rdi
    mov $0x42, %rsi                /* O_CREAT | O_RDWR */
    mov $0x1B6, %rdx               /* 0666 */
    syscall

    cmp $0, %rax
    jl .Lm8_error
    mov %rax, %r8                  /* r8 = fd */

    /* 2. sys_write(fd, msg, len) */
    mov $1, %rax                   /* SYS_write */
    mov %r8, %rdi
    lea ext4_msg(%rip), %rsi
    mov $ext4_msg_len, %rdx
    syscall

    cmp $ext4_msg_len, %rax
    jne .Lm8_error

    /* 3. sys_lseek(fd, 0, SEEK_SET = 0) */
    mov $8, %rax                   /* SYS_lseek */
    mov %r8, %rdi
    xor %rsi, %rsi                 /* offset = 0 */
    xor %rdx, %rdx                 /* whence = SEEK_SET */
    syscall

    cmp $0, %rax
    jne .Lm8_error

    /* 4. sys_read(fd, stack_buf, len) */
    mov $0, %rax                   /* SYS_read */
    mov %r8, %rdi
    lea -64(%rsp), %rsi
    mov $ext4_msg_len, %rdx
    syscall

    cmp $ext4_msg_len, %rax
    jne .Lm8_error

    /* 5. sys_close(fd) */
    mov $3, %rax                   /* SYS_close */
    mov %r8, %rdi
    syscall

    /* Exit with success code 88 */
    mov $60, %rax                  /* SYS_exit */
    mov $88, %rdi                  /* status = 88 */
    syscall

.Lm8_error:
    mov $60, %rax
    mov $127, %rdi                 /* error status 127 */
    syscall

.Lm8_loop:
    jmp .Lm8_loop

path_ext4_file:
    .asciz "/test_ext4.txt"

ext4_msg:
    .ascii "Hello ext4 from Ring 3!\n"
ext4_msg_end:
    .set ext4_msg_len, ext4_msg_end - ext4_msg

user_m8_blob_end:
