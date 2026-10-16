.global _start
.section .text
_start:
    /* sys_write(1, msg, len) */
    mov $1, %rax
    mov $1, %rdi
    lea msg(%rip), %rsi
    mov $msg_len, %rdx
    syscall

    /* sys_exit(77) */
    mov $60, %rax
    mov $77, %rdi
    syscall

1:  jmp 1b

.section .rodata
msg:
    .ascii "[ELF64] Hello from /bin/hello! Process executed via fork + execve!\n"
msg_end:
    .set msg_len, msg_end - msg
