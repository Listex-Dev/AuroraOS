/*
 * Ring 3 User Mode Test for VFS Syscalls (open, write, lseek, read, close)
 */

.section .rodata
.global user_vfs_blob_start
.global user_vfs_blob_end

user_vfs_blob_start:
    /* 1. sys_open("/user_vfs.txt", O_CREAT | O_RDWR = 0x42, 0644 = 0x1A4) */
    mov $2, %rax               /* SYS_open */
    lea vfs_file_path(%rip), %rdi
    mov $0x42, %rsi            /* O_CREAT | O_RDWR */
    mov $0x1A4, %rdx           /* 0644 mode */
    syscall

    cmp $0, %rax
    jl .Lvfs_error
    mov %rax, %r8              /* r8 = fd */

    /* 2. sys_write(fd, msg, len) */
    mov $1, %rax               /* SYS_write */
    mov %r8, %rdi              /* fd */
    lea vfs_msg(%rip), %rsi    /* buf */
    mov $vfs_msg_len, %rdx     /* count */
    syscall

    cmp $vfs_msg_len, %rax
    jne .Lvfs_error

    /* 3. sys_lseek(fd, 0, SEEK_SET = 0) */
    mov $8, %rax               /* SYS_lseek */
    mov %r8, %rdi              /* fd */
    xor %rsi, %rsi             /* offset = 0 */
    xor %rdx, %rdx             /* whence = SEEK_SET (0) */
    syscall

    cmp $0, %rax
    jne .Lvfs_error

    /* 4. sys_read(fd, user_stack_buf, len) */
    mov $0, %rax               /* SYS_read */
    mov %r8, %rdi              /* fd */
    lea -64(%rsp), %rsi        /* buffer on stack */
    mov $vfs_msg_len, %rdx     /* count */
    syscall

    cmp $vfs_msg_len, %rax
    jne .Lvfs_error

    /* 5. sys_close(fd) */
    mov $3, %rax               /* SYS_close */
    mov %r8, %rdi              /* fd */
    syscall

    /* Exit with success code 88 */
    mov $60, %rax              /* SYS_exit */
    mov $88, %rdi              /* status = 88 */
    syscall

.Lvfs_error:
    mov $60, %rax
    mov $123, %rdi             /* error status 123 */
    syscall

.Lloop:
    jmp .Lloop

vfs_file_path:
    .asciz "/user_vfs.txt"

vfs_msg:
    .ascii "UserMode VFS OK!"
vfs_msg_end:
    .set vfs_msg_len, vfs_msg_end - vfs_msg

user_vfs_blob_end:
